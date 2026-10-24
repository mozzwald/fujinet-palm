#!/usr/bin/env python3
"""Prism result collection. Standard-library only; no raw USB or MCU reset."""
import argparse
import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import select
import shutil
import struct
import subprocess
import sys
import termios
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'palm/apps/visorcheck'
DEFAULT_ROM = ROOT / 'palm/apps/visorbus/build/visorbus.rom'
DB = 'VisorCheckResults'
HEADER = 144
SPACED_HEADER = 160
READ_SPACING = ('tick', '100us', '10us', '1us', 'none')
CAPTURE_PROFILES = ('guarded', 'burst64')
SPACING_ITERATIONS = (0, 330, 33, 3, 0)
KIND = {'rom': 1, 'synthetic-pass': 2, 'synthetic-fail': 3,
        'constant-zero': 4, 'constant-ones': 5, 'rom-prefix': 6}
BUS_KINDS = (1, 4, 5, 6)
SHORT_BYTES = 128
MARKERS = ('drive', 'request', 'reply', 'preload')

def expected_payload(rom, kind):
    if kind in (4, 5, 6):
        if len(rom) < SHORT_BYTES:
            raise CollectionError('reference too short for latency diagnostic')
        return bytes([255 if kind == 5 else 0]) * SHORT_BYTES if kind != 6 else rom[:SHORT_BYTES]
    return rom
STATUS = {0: 'INCOMPLETE', 1: 'PASS', 2: 'FAIL', 3: 'INCOMPLETE', 4: 'INCOMPLETE'}
FIELDS = ('sequence kind status expected_size expected_crc reference_crc completed crc '
          'mismatch_count reason error original requested applied restored setup_error '
          'restore_error started ended app_version').split()

class CollectionError(Exception):
    pass


def crc(data):
    return zlib.crc32(data) & 0xffffffff


def atomic_json(path, value):
    path = Path(path)
    temp = path.with_suffix('.tmp')
    with temp.open('x') as f:
        json.dump(value, f, indent=2)
        f.write('\n')
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp, path)
    fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def make_pdb(name, records, db_type=b'VRSL'):
    if not records or len(records) > 65535 or len(name.encode('ascii')) > 31:
        raise CollectionError('invalid database record count/name')
    header = bytearray(78)
    header[:32] = name.encode('ascii').ljust(32, b'\0')
    struct.pack_into('>H', header, 34, 1)
    header[60:64], header[64:68] = db_type, b'FNVC'
    struct.pack_into('>I', header, 68, len(records) + 1)
    struct.pack_into('>H', header, 76, len(records))
    offset = 78 + 8 * len(records) + 2
    entries = bytearray()
    for index, record in enumerate(records, 1):
        entries.extend(struct.pack('>I', offset) + b'\0' + index.to_bytes(3, 'big'))
        offset += len(record)
    return bytes(header + entries + b'\0\0') + b''.join(records)


def pdb_records(data, name=DB, db_type=b'VRSL'):
    if len(data) < 80 or data[:32].split(b'\0')[0] != name.encode():
        raise CollectionError('wrong/truncated Palm database')
    attrs = struct.unpack_from('>H', data, 32)[0]
    # palm-sync reads attributes through the open DLP handle and preserves
    # dmHdrAttrOpen (0x8000) in its exported header. It does not describe an
    # incomplete transfer. Validate the record envelope and payload separately.
    if attrs & 1 or data[60:68] != db_type + b'FNVC':
        raise CollectionError('wrong database type/creator or resource database')
    if struct.unpack_from('>II', data, 52) != (0, 0) or struct.unpack_from('>I', data, 72)[0]:
        raise CollectionError('unsupported app/sort blocks or chained record list')
    count = struct.unpack_from('>H', data, 76)[0]
    base = 78 + 8 * count
    if not count or base > len(data):
        raise CollectionError('missing/truncated record table')
    offsets, ids = [], set()
    for i in range(count):
        at = 78 + i * 8
        offset = struct.unpack_from('>I', data, at)[0]
        uid = int.from_bytes(data[at+5:at+8], 'big')
        if data[at+4] & 0x88 or not uid or uid in ids:
            raise CollectionError('deleted/busy/duplicate Palm record')
        ids.add(uid)
        if offset < base or offset >= len(data) or (offsets and offset <= offsets[-1]):
            raise CollectionError('invalid Palm record offsets')
        offsets.append(offset)
    return [data[a:b] for a, b in zip(offsets, offsets[1:] + [len(data)])]


def config_record(session, rom):
    body = (b'VCC1' + struct.pack('>I', 1) + bytes.fromhex(session) +
            struct.pack('>II', len(rom), crc(rom)) + hashlib.sha256(rom).digest())
    return body + struct.pack('>I', crc(body))


def fixture_record(session, rom, sequence, kind, status, actual=b'',
                   read_spacing=None, capture_profile='guarded'):
    expected_rom = expected_payload(rom, kind)
    mismatches = [(i, int.from_bytes(expected_rom[i:i+2], 'big'),
                   int.from_bytes(actual[i:i+2], 'big'))
                  for i in range(0, len(actual), 2) if expected_rom[i:i+2] != actual[i:i+2]]
    header_size = SPACED_HEADER if read_spacing is not None else HEADER
    header = bytearray(header_size)
    header[:8] = (b'VCR2' + struct.pack('>I', 2)) if read_spacing is not None else (b'VCR1' + struct.pack('>I', 1))
    header[8:24] = bytes.fromhex(session)
    header[24:56] = hashlib.sha256(rom).digest()
    values = [sequence, kind, status, len(expected_rom), crc(expected_rom), crc(rom), len(actual),
              crc(actual), len(mismatches), 0, 0, 0, 0, 0, 0, 0, 0, 1,
              0 if status == 0 else 2, 0x600 if read_spacing is not None else 0x500 if kind >= 4 else 0x400]
    if kind in BUS_KINDS and status != 0:
        values[11] = values[13] = 330
    struct.pack_into('>20I', header, 56, *values)
    length = header_size + len(actual) + 8 * len(mismatches) + 4
    struct.pack_into('>II', header, 136, length, header_size)
    if read_spacing is not None:
        mode, profile = READ_SPACING.index(read_spacing), CAPTURE_PROFILES.index(capture_profile)
        struct.pack_into('>4I', header, 144, mode, profile, SPACING_ITERATIONS[mode], 64 if profile else 1)
    body = bytes(header) + actual + b''.join(struct.pack('>IHH', *m) for m in mismatches)
    return body + struct.pack('>I', crc(body))


def decode_record(raw, rom):
    if len(raw) < HEADER + 4 or raw[:8] not in (b'VCR1\0\0\0\1', b'VCR2\0\0\0\2'):
        raise CollectionError('unknown/truncated result record')
    version = struct.unpack_from('>I', raw, 4)[0]
    size, header = struct.unpack_from('>II', raw, 136)
    if size != len(raw) or header != (HEADER if version == 1 else SPACED_HEADER) or len(raw) < header+4 or crc(raw[:-4]) != struct.unpack_from('>I', raw, len(raw)-4)[0]:
        raise CollectionError('record length/integrity failure')
    d = dict(zip(FIELDS, struct.unpack_from('>20I', raw, 56)))
    d['session'], d['rom_sha256'] = raw[8:24].hex(), raw[24:56].hex()
    if (not d['sequence'] or d['kind'] not in KIND.values() or d['status'] not in STATUS or
        d['app_version'] not in ((0x400, 0x401, 0x500) if version == 1 else (0x600, 0x601)) or d['expected_size'] > 12000 or d['expected_size'] & 1 or
        not d['expected_size'] or d['completed'] > d['expected_size'] or d['completed'] & 1 or
        len(raw) != header + d['completed'] + 8*d['mismatch_count'] + 4):
        raise CollectionError('unsupported/inconsistent result fields')
    if d['kind'] >= 4 and d['app_version'] not in (0x500, 0x600, 0x601):
        raise CollectionError('latency test requires checker0.5.x or0.6.x')
    mode, profile, iterations, batch = struct.unpack_from('>4I', raw, 144) if version == 2 else (0, 0, 0, 1)
    if (mode >= len(READ_SPACING) or profile >= len(CAPTURE_PROFILES) or
            iterations != SPACING_ITERATIONS[mode] or batch != (64 if profile else 1) or
            (d['kind'] not in BUS_KINDS and (mode or profile))):
        raise CollectionError('invalid spacing/capture metadata')
    d.update(record_version=version, header_size=header, spacing_mode=mode,
             capture_mode=profile, delay_iterations=iterations, batch_words=batch,
             read_spacing=READ_SPACING[mode], capture_profile=CAPTURE_PROFILES[profile])
    actual = raw[header:header+d['completed']]
    if crc(actual) != d['crc']:
        raise CollectionError('captured bytes disagree with bus CRC')
    d['mismatches'] = [dict(zip(('offset', 'expected', 'actual'), values))
                       for values in struct.iter_unpack('>IHH', raw[header+d['completed']:-4])]
    # Old sessions may have a different ROM. Validate their envelope but never
    # use them as current evidence. Current-session verification follows below.
    if d['rom_sha256'] == hashlib.sha256(rom).hexdigest():
        expected_rom = expected_payload(rom, d['kind'])
        if d['expected_size'] != len(expected_rom) or d['expected_crc'] != crc(expected_rom) or d['reference_crc'] != crc(rom):
            raise CollectionError('ROM/reference identity mismatch')
        expected = [dict(offset=i, expected=int.from_bytes(expected_rom[i:i+2], 'big'),
                         actual=int.from_bytes(actual[i:i+2], 'big'))
                    for i in range(0, len(actual), 2) if actual[i:i+2] != expected_rom[i:i+2]]
        if expected != d['mismatches']:
            raise CollectionError('missing/incorrect mismatch records')
        clean = (d['completed'] == len(expected_rom) and not expected and d['crc'] == crc(expected_rom))
        if d['status'] in (1, 2) and (d['completed'] != len(expected_rom) or (d['status'] == 1) != clean):
            raise CollectionError('invalid PASS/FAIL claim')
    if d['status'] in (1, 2) and d['rom_sha256'] == hashlib.sha256(rom).hexdigest():
        if d['kind'] == 2 and actual != rom:
            raise CollectionError('synthetic PASS pattern differs from reference')
        if d['kind'] == 3 and actual != bytes(b ^ (i&1) for i,b in enumerate(rom)):
            raise CollectionError('synthetic FAIL pattern is not the known all-word mismatch')
    if d['requested'] or d['restored'] or d['setup_error'] or d['restore_error']:
        raise CollectionError('unexpected timing change/error in automation result')
    if d['status'] in (1, 2) and (d['reason'] or d['error']):
        raise CollectionError('completed result contains an access error')
    if d['status'] in (1, 2) and ((d['kind'] in BUS_KINDS and (not d['original'] or d['applied'] != d['original'])) or
                                  (d['kind'] not in BUS_KINDS and (d['original'] or d['applied']))):
        raise CollectionError('unexpected normal/synthetic timing metadata')
    if d['status'] == 0 and (d['completed'] or d['mismatch_count'] or d['ended']):
        raise CollectionError('invalid STARTED marker')
    d['result'] = STATUS[d['status']]
    return d


def decode_pdb(data, session, rom):
    runs = {}
    for raw in pdb_records(data):
        d = decode_record(raw, rom)
        if d['session'] != session:
            continue
        if d['rom_sha256'] != hashlib.sha256(rom).hexdigest():
            raise CollectionError('current-session ROM SHA mismatch')
        pair = runs.setdefault(d['sequence'], {})
        key = 'start' if d['status'] == 0 else 'final'
        if key in pair:
            raise CollectionError('duplicate run marker/final')
        pair[key] = d
    if not runs:
        raise CollectionError('no results for the expected host session')
    result = {}
    for seq, pair in runs.items():
        if 'start' not in pair:
            raise CollectionError('final result without STARTED marker')
        final = pair.get('final', pair['start'])
        pairing = ('kind', 'started', 'app_version', 'record_version', 'read_spacing',
                   'capture_profile', 'delay_iterations', 'batch_words')
        if any(final[k] != pair['start'][k] for k in pairing):
            raise CollectionError('run markers disagree')
        result[seq] = final
    return result


def usb_identity(path):
    for ancestor in [path.resolve(), *path.resolve().parents]:
        try:
            return ((ancestor/'idVendor').read_text().strip(),
                    (ancestor/'idProduct').read_text().strip())
        except OSError:
            pass
    return None


def find_palm():
    for tty in Path('/sys/class/tty').glob('ttyUSB*'):
        device = tty/'device'
        if usb_identity(device) != ('082d', '0100'):
            continue
        try:
            port = (device/'port_number').read_text().strip()
        except OSError:
            port = '1' if tty.name == 'ttyUSB1' else ''
        if port == '1' and (Path('/dev')/tty.name).exists():
            return str(Path('/dev')/tty.name)
    return None


def wait_palm(timeout):
    print('Press HotSync on the Prism; waiting for verified Handspring HotSync port.', flush=True)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        port = find_palm()
        if port:
            return port
        time.sleep(.1)
    raise CollectionError('HotSync timeout; Palm results remain on the device')


def transfer(session_dir, output, mode, names, timeout, transport='pilot-link'):
    port = wait_palm(timeout)
    if transport == 'pilot-link':
        tool = shutil.which('pilot-xfer')
        if not tool:
            raise CollectionError('pilot-xfer is not installed')
        command = [tool, '-p', port, '-t', str(max(1,int(timeout))), mode, *map(str, names)]
    else:
        cli = ROOT/'tools/palm-sync/dist/bin/cli.js'
        if not cli.exists() or not shutil.which('node'):
            raise CollectionError('existing palm-sync fallback is not built')
        command = ['node', str(cli), 'push' if mode == '--install' else 'pull',
                   '--serial', port]
        if mode == '--fetch':
            command += ['--output-dir',str(output)]
        command += list(map(str,names))
    atomic_json(output/'transfer-command.json', command)
    with (output/'transport.log').open('xb') as log:
        try:
            proc = subprocess.run(command, cwd=output, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            raise CollectionError('HotSync transfer timed out; original results retained') from exc
    if proc.returncode:
        raise CollectionError(f'HotSync transfer failed ({proc.returncode}); see {output}/transport.log')


class RP:
    def __init__(self, port, log):
        if usb_identity(Path('/sys/class/tty')/Path(port).name/'device') != ('2e8a', '0009'):
            raise CollectionError('RP port is not the expected Raspberry Pi USB CDC device')
        fuser = shutil.which('fuser')
        if not fuser:
            raise CollectionError('fuser required for exclusive serial preflight')
        if subprocess.run([fuser, port], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0:
            raise CollectionError('RP serial port busy; close minicom/other monitor')
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self.log = open(log, 'xb')
        try:
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            fcntl.ioctl(self.fd, termios.TIOCEXCL)
            attrs = termios.tcgetattr(self.fd)
            attrs[0] = attrs[1] = attrs[3] = 0
            attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
            attrs[4] = attrs[5] = termios.B115200
            attrs[6][termios.VMIN] = attrs[6][termios.VTIME] = 0
            termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        except Exception:
            self.close()
            raise

    def close(self):
        if getattr(self, 'fd', None) is not None:
            with contextlib.suppress(OSError):
                fcntl.ioctl(self.fd, termios.TIOCNXCL)
            os.close(self.fd)
            self.fd = None
        if getattr(self, 'log', None):
            self.log.close()

    def read(self, seconds):
        result = b''
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if select.select([self.fd], [], [], max(0, end-time.monotonic()))[0]:
                chunk = os.read(self.fd, 4096)
                if not chunk:
                    raise CollectionError('RP USB disconnected')
                self.log.write(chunk); self.log.flush()
                result += chunk
        return result.decode('ascii', 'replace')

    def send(self, command):
        self.log.write(('\nHOST> '+command+'\n').encode()); self.log.flush()
        os.write(self.fd, command.encode())

    def status(self):
        self.read(.05)
        self.send('s')
        text = ''
        required = {'rom_ready','ready','stopped','arm_pending','run','requests','posted',
                    'completed','limit','enabled','not_enabled','address_errors','anomalies'}
        end = time.monotonic()+2
        while time.monotonic()<end:
            text += self.read(.05)
            fields = dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)',text))
            if required <= fields.keys():
                return fields
        raise CollectionError('incomplete/unrecognized RP status response')

    def stop(self):
        self.send('x')
        for _ in range(15):
            status = self.status()
            if status['stopped'] and not status['arm_pending']:
                return status
            time.sleep(.1)
        raise CollectionError('RP stop not confirmed; do not touch/rearm hardware')

    def _identify(self, rom, banner):
        self.stop()
        # DTR re-announces the existing core-0 console banner; not a chip reset.
        try:
            fcntl.ioctl(self.fd, termios.TIOCMBIC, struct.pack('I', termios.TIOCM_DTR))
            time.sleep(.1)
            fcntl.ioctl(self.fd, termios.TIOCMBIS, struct.pack('I', termios.TIOCM_DTR))
        except OSError as exc:
            raise CollectionError('cannot request firmware identity via CDC DTR') from exc
        text = self.read(1)
        match = re.search(r'ROM=(\d+) CRC32=([0-9a-fA-F]{8})', text)
        if banner not in text or not match or (int(match[1]),int(match[2],16)) != (len(rom),crc(rom)):
            raise CollectionError('RP firmware/ROM identity not verified; no test armed')
        self.identity_text = text
        return self.status()

    def identify(self, rom):
        return self._identify(rom, 'FujiSpring EXPERIMENTAL CS0 ROM / bounded reads / 150MHz')

    def identify_latency(self, rom, response, marker):
        self._identify(rom, 'FujiSpring LATENCY DIAGNOSTIC / bounded reads / 150MHz')
        commands = {'cpu-rom': 'R', 'cpu-zero': 'Z', 'cpu-ones': 'F',
                    'pio-zero': 'z', 'pio-ones': 'f'}
        if response not in commands or marker not in MARKERS:
            raise CollectionError('unsupported latency configuration')
        self.send(commands[response]); self.read(.1)
        self.send(str(MARKERS.index(marker))); self.read(.1)
        self.send('q')
        text = self.read(.3)
        match = re.search(r'LATENCY response=([a-z-]+) marker=([a-z-]+) words=(\d+)', text)
        if not match or match.groups() != (response, marker, str(SHORT_BYTES//2)):
            raise CollectionError('latency response/profile readback mismatch; no test armed')
        return self.status(), dict(response=response, marker=marker, words=SHORT_BYTES//2)

    def identify_latency_full(self, rom, marker):
        status, config = self.identify_latency(rom, 'cpu-rom', marker)
        capability = re.search(r'(?:^|\n)ENGINE=rx-first-v2 full_words=(\d+)\r?(?:\n|$)',
                               getattr(self, 'identity_text', ''))
        if not capability or int(capability[1]) != len(rom)//2:
            raise CollectionError('full ROM requires verified rx-first-v2 engine and exact full_words capability; no test armed')
        return status, dict(config, words=len(rom)//2, engine='rx-first-v2', full_image=True)

    def arm(self, previous, words, command='b'):
        if not (previous['rom_ready'] and previous['ready'] and previous['stopped'] and not previous['arm_pending']):
            raise CollectionError('RP not ready to arm')
        if command not in ('b', 'B'):
            raise CollectionError('unsupported RP arm command')
        self.send(command)
        status = self.status()
        if (status['run'] != previous['run']+1 or status['stopped'] or status['arm_pending'] or
            status['limit'] != words or any(status[k] for k in ('requests','posted','completed','enabled','not_enabled','address_errors','anomalies'))):
            raise CollectionError('RP arm/read-budget confirmation failed')
        return status

    def wait(self, armed, timeout, collect_on_hotsync=False):
        end = time.monotonic()+timeout
        while time.monotonic()<end:
            status = self.status()
            if status['run'] != armed['run'] or status['limit'] != armed['limit']:
                raise CollectionError('RP run identity changed during test')
            if status['stopped'] and not status['arm_pending']:
                self.send('e')
                self.read(.5)
                return status
            if collect_on_hotsync and find_palm():
                # A fast Palm batch can finish despite missed RP requests.
                # Preserve partial counters and collect its saved result now,
                # rather than waiting indefinitely for reads that already ended.
                status = self.stop()
                status['host_stopped_for_hotsync'] = 1
                self.send('e'); self.read(.5)
                return status
            time.sleep(1)
        raise CollectionError('ROM read timeout; test not complete')


def rp_port(explicit):
    if explicit:
        return explicit
    ports = [str(Path('/dev')/p.name) for p in Path('/sys/class/tty').glob('ttyACM*')
             if usb_identity(p/'device') == ('2e8a','0009')]
    if len(ports) != 1:
        raise CollectionError('need exactly one RP USB console, or specify --rp')
    return ports[0]


def setup(args):
    directory = args.directory.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    rom, prc = args.rom.read_bytes(), args.prc.read_bytes()
    if not rom or len(rom) & 1 or len(rom)>12000:
        raise CollectionError('unsupported ROM size')
    session = secrets.token_hex(16)
    (directory/'reference.rom').write_bytes(rom)
    (directory/'visorcheck.prc').write_bytes(prc)
    control = make_pdb('VisorCheckControl',[config_record(session,rom)],b'VCTL')
    (directory/'VisorCheckControl.pdb').write_bytes(control)
    for kind, name in [(2,'pass-fixture.pdb'),(3,'fail-fixture.pdb')]:
        actual = rom if kind == 2 else bytes(b ^ (i&1) for i,b in enumerate(rom))
        records = [fixture_record(session,rom,1,kind,0),
                   fixture_record(session,rom,1,kind,1 if kind==2 else 2,actual)]
        (directory/name).write_bytes(make_pdb(DB,records))
    atomic_json(directory/'session.json', dict(version=1,session=session,last_sequence=0,
                rom_sha256=hashlib.sha256(rom).hexdigest(),prc_sha256=hashlib.sha256(prc).hexdigest(),
                transport_verified=False,transport='pilot-link',installed=False,pending=None,validation=[]))
    print(f'Session prepared: {directory}\nNo hardware accessed. Next: install, then probe.')


def load_session(directory):
    directory = directory.resolve()
    state = json.loads((directory/'session.json').read_text())
    rom = (directory/'reference.rom').read_bytes()
    if state['version'] != 1 or state['rom_sha256'] != hashlib.sha256(rom).hexdigest():
        raise CollectionError('invalid session/reference identity')
    if len(bytes.fromhex(state['session'])) != 16:
        raise CollectionError('invalid session token')
    return directory, state, rom


def report(directory, d, rp=None):
    result = dict(d)
    result['rp'] = rp
    if d['kind'] in BUS_KINDS:
        if not rp or not d.get('rp_armed'):
            result['result'] = 'COLLECTION ERROR'
            result['collection_error'] = 'ROM evidence lacks its paired RP arming/final diagnostics'
        else:
            armed = d['rp_armed']
            config = d.get('rp_config')
            if d['kind'] >= 4:
                allowed = {4: ('cpu-zero', 'pio-zero'), 5: ('cpu-ones', 'pio-ones'), 6: ('cpu-rom',)}[d['kind']]
                if (not config or config.get('response') not in allowed or
                        config.get('marker') not in MARKERS or config.get('words') != SHORT_BYTES//2):
                    result['result'] = 'COLLECTION ERROR'
                    result['collection_error'] = 'missing or mismatched latency mode evidence'
            elif d.get('latency_full_rom') or config:
                if (not config or config.get('response') != 'cpu-rom' or
                        config.get('marker') not in MARKERS or
                        config.get('engine') != 'rx-first-v2' or
                        config.get('full_image') is not True or
                        config.get('words') != d['expected_size']//2):
                    result['result'] = 'COLLECTION ERROR'
                    result['collection_error'] = 'missing or mismatched full ROM engine/capability evidence'
            wanted_posted = 0 if config and config.get('response','').startswith('pio-') else d['expected_size']//2
            result['rp_address_validation'] = ('not sampled in PIO constant mode' if config and config.get('response','').startswith('pio-') else 'CPU sequential address check')
            if (rp['run'] != armed['run'] or rp['limit'] != d['expected_size']//2 or
                armed.get('limit') != d['expected_size']//2 or
                not rp['stopped'] or rp['arm_pending']):
                result['result'] = 'COLLECTION ERROR'
                result['collection_error'] = 'RP identity/budget/stop mismatch'
            elif rp['posted'] != wanted_posted or any(rp[k] != d['expected_size']//2 for k in ('requests','completed','enabled')) or any(rp[k] for k in ('not_enabled','address_errors','anomalies')):
                result['result'] = 'INCOMPLETE' if rp['completed'] < d['expected_size']//2 else 'COLLECTION ERROR'
                result['collection_error'] = 'RP counters/anomalies do not qualify the requested complete ROM run'
    result['qualification'] = 'Readback only; no ROM execution, sampling margin or rapid-cycle qualification'
    result['test_kind'] = next(k for k,v in KIND.items() if v==d['kind'])
    atomic_json(directory/'report.json',result)
    lines = [f"# {result['result']} — {result['test_kind']}", '',
             f"Run: {d['session']}.{d['sequence']}",
             f"CRC: {d['crc']:08X}; expected {d['expected_crc']:08X}",
             f"Bytes: {d['completed']}/{d['expected_size']}; mismatches: {d['mismatch_count']}",
             f"Timing ns: original={d['original']}, applied={d['applied']}; no timing changes requested.",
             '', '| Offset | Expected | Actual |', '| --- | --- | --- |']
    lines[7:7] = [f"Requested spacing: {d['read_spacing']}; capture: {d['capture_profile']}; batch: {d['batch_words']} words.",
                  'Requested delay is not a measured bus gap or setup/hold margin.']
    if d.get('rp_config'):
        config = d['rp_config']
        lines[7:7] = [f"Responder: {config['response']}; marker: {config['marker']}; budget: {config['words']} words.",
                      f"Address validation: {result.get('rp_address_validation', 'unqualified')}."]
    lines += [f"| {m['offset']:04X} | {m['expected']:04X} | {m['actual']:04X} |" for m in d['mismatches']]
    if result.get('collection_error'):
        lines += ['',result['collection_error']]
    (directory/'report.md').write_text('\n'.join(lines)+'\n')
    return result


def collect(directory, state, rom, timeout):
    pending = state['pending']
    if not pending:
        raise CollectionError('no pending host run; do not associate an arbitrary result')
    output = directory/pending['directory']
    download = output/('download-'+secrets.token_hex(4))
    download.mkdir()
    transfer(directory,download,'--fetch',[DB],timeout,transport=state.get('transport','pilot-link'))
    data = (download/(DB+'.pdb')).read_bytes()
    results = decode_pdb(data,state['session'],rom)
    seq = pending['sequence']
    if seq not in results or max(results) != seq:
        raise CollectionError('missing/stale/unexpected Palm run sequence; raw PDB retained')
    d = results[seq]
    if d['kind'] != KIND[pending['kind']]:
        raise CollectionError('Palm test type differs from armed host run; raw PDB retained')
    if (d['read_spacing'] != pending.get('read_spacing', 'tick') or
            d['capture_profile'] != pending.get('capture_profile', 'guarded')):
        raise CollectionError('Palm spacing/capture differs from armed host settings; raw PDB retained, do not repeat the read')
    if pending.get('latency_full_rom'):
        d['latency_full_rom'] = True
    rp = None
    if (output/'rp-final.json').exists():
        rp=json.loads((output/'rp-final.json').read_text())
        d['rp_armed']=json.loads((output/'rp-armed.json').read_text())
        if (output/'rp-config.json').exists():
            d['rp_config']=json.loads((output/'rp-config.json').read_text())
    result = report(output,d,rp)
    if result['result'] == 'COLLECTION ERROR':
        raise CollectionError(result['collection_error'])
    if d['kind'] in (2,3) and d['status'] in (1,2):
        state.setdefault('validation',[]).append(dict(sequence=seq,kind=d['kind'],result=result['result']))
    if d['kind'] in (4,5) and result['result']=='PASS':
        state.setdefault('spacing_validation',[]).append(dict(sequence=seq,kind=d['kind'],
            read_spacing=d['read_spacing'],capture_profile=d['capture_profile'],words=64,
            app_version=d['app_version']))
    state['last_app_version']=d['app_version']
    state['last_sequence'],state['pending']=seq,None
    atomic_json(directory/'session.json',state)
    print(f"{result['result']}: {output/'report.md'}",flush=True)
    return 0 if result['result']=='PASS' else 1


def execute(args):
    if args.command=='doctor':
        pilot=shutil.which('pilot-xfer')
        print(json.dumps({'pilot_xfer':pilot,'fuser':shutil.which('fuser'),
              'prc_exists':(APP/'visorcheck.prc').exists(),
              'palm_hotsync_port':find_palm(),'rp_candidates':[p.name for p in Path('/sys/class/tty').glob('ttyACM*')
               if usb_identity(p/'device')==('2e8a','0009')], 'hardware_opened':False},indent=2))
        return 0 if pilot else 2
    if args.command=='setup':
        setup(args); return 0
    if args.command=='decode':
        runs=decode_pdb(args.pdb.read_bytes(),args.session,args.rom.read_bytes())
        print(json.dumps(runs,indent=2));return 0
    directory,state,rom=load_session(args.directory)
    with (directory/'session.lock').open('a') as lock:
        try: fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError as exc: raise CollectionError('another collector owns this session') from exc
        directory,state,rom=load_session(directory)
        if args.command in ('install','probe'):
            output=directory/(args.command+'-'+secrets.token_hex(4));output.mkdir()
            transport=args.transport or state.get('transport','pilot-link')
            if args.command=='install':
                if state['pending']:
                    raise CollectionError('cannot install while a run is pending')
                prc=directory/'visorcheck.prc'
                if hashlib.sha256(prc.read_bytes()).hexdigest()!=state['prc_sha256']:
                    raise CollectionError('prepared PRC changed')
                transfer(directory,output,'--install',[prc,directory/'VisorCheckControl.pdb'],args.timeout,transport=transport)
                state['installed']=True
                state['transport_verified']=False
            else:
                if not state['installed']:
                    raise CollectionError('install session control database before probe')
                transfer(directory,output,'--fetch',['VisorCheckControl'],args.timeout,transport=transport)
                raw=pdb_records((output/'VisorCheckControl.pdb').read_bytes(),'VisorCheckControl',b'VCTL')
                if raw != [config_record(state['session'],rom)]:
                    raise CollectionError('control PDB did not round-trip intact')
                state['transport_verified']=True
            state['transport']=transport
            atomic_json(directory/'session.json',state)
            print(f'{args.command}: successful; logs in {output}');return 0
        if args.command=='collect':
            return collect(directory,state,rom,args.timeout)
        if not state['transport_verified']:
            raise CollectionError('first verify pilot-xfer retrieval with probe')
        if KIND[args.kind] in BUS_KINDS:
            validated = state.get('validation',[])
            if len(validated) < 5 or not {2,3} <= {v['kind'] for v in validated}:
                raise CollectionError('first collect at least five synthetic runs, including PASS and FAIL')
        if state['pending']:
            raise CollectionError('previous run pending: use collect; never overwrite uncollected evidence')
        read_spacing = getattr(args, 'read_spacing', 'tick')
        capture_profile = getattr(args, 'capture_profile', 'guarded')
        fast_collection = read_spacing != 'tick' or capture_profile != 'guarded'
        if args.kind == 'rom' and fast_collection:
            validated = {v['kind'] for v in state.get('spacing_validation', [])
                         if v['read_spacing']==read_spacing and v['capture_profile']==capture_profile and v['words']==64
                         and v.get('app_version',0x600)==state.get('last_app_version',0x600)}
            if not {4,5} <= validated:
                raise CollectionError('first collect passing Zero and Ones64 at this exact spacing/capture before full ROM')
        seq=state['last_sequence']+1
        output=directory/f'run-{seq:06d}-{secrets.token_hex(4)}';output.mkdir()
        pending=dict(sequence=seq,kind=args.kind,directory=output.name,
                     read_spacing=read_spacing,capture_profile=capture_profile)
        full_latency = getattr(args, 'latency_full_rom', False)
        if full_latency:
            pending['latency_full_rom'] = True
        rp=None
        atomic_json(output/'host-run.json',dict(session=state['session'],sequence=seq,
                    kind=args.kind,read_spacing=read_spacing,capture_profile=capture_profile,
                    started_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())))
        try:
            if KIND[args.kind] in BUS_KINDS:
                rp=RP(rp_port(args.rp),output/'rp-console.log')
                if full_latency:
                    before, config = rp.identify_latency_full(rom, getattr(args, 'marker', 'drive'))
                    atomic_json(output/'rp-config.json',config)
                elif KIND[args.kind] >= 4:
                    responder = getattr(args, 'responder', 'cpu')
                    if args.kind == 'rom-prefix' and responder != 'cpu':
                        raise CollectionError('ROM prefix requires CPU responder')
                    suffix = {'constant-zero': 'zero', 'constant-ones': 'ones', 'rom-prefix': 'rom'}[args.kind]
                    response = responder + '-' + suffix
                    before, config = rp.identify_latency(rom, response, getattr(args, 'marker', 'drive'))
                    atomic_json(output/'rp-config.json',config)
                else:
                    if getattr(args, 'responder', 'cpu') != 'cpu' or getattr(args, 'marker', 'drive') != 'drive':
                        raise CollectionError('profiles require a short latency test kind')
                    before=rp.identify(rom)
                atomic_json(output/'rp-before.json',before)
                state['pending']=pending
                atomic_json(directory/'session.json',state)
                words = len(expected_payload(rom, KIND[args.kind]))//2
                armed = rp.arm(before, words, command='B') if full_latency else rp.arm(before, words)
                atomic_json(output/'rp-armed.json',armed)
                button = {'rom': 'Run', 'rom-prefix': 'ROM64', 'constant-zero': 'Zero', 'constant-ones': 'Ones'}[args.kind]
                print(f"RP armed run={armed['run']} limit={armed['limit']}. Tap {button}, then Read on Prism.",flush=True)
                final = rp.wait(armed,args.read_timeout,collect_on_hotsync=True) if fast_collection else rp.wait(armed,args.read_timeout)
                atomic_json(output/'rp-final.json',final)
            else:
                state['pending']=pending
                atomic_json(directory/'session.json',state)
                print('No RP access or bus reads. Tap '+('Pass' if args.kind=='synthetic-pass' else 'Fail')+' on Prism 0.4.x.',flush=True)
            return collect(directory,state,rom,args.timeout)
        finally:
            if rp:
                try:
                    stopped=rp.stop()
                    if (output/'rp-armed.json').exists() and not (output/'rp-final.json').exists():
                        atomic_json(output/'rp-final.json',stopped)
                    atomic_json(output/('rp-stop-'+secrets.token_hex(4)+'.json'),stopped)
                finally: rp.close()


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    sub.add_parser('doctor')
    setup_p=sub.add_parser('setup'); setup_p.add_argument('directory',type=Path)
    setup_p.add_argument('--rom',type=Path,default=DEFAULT_ROM)
    setup_p.add_argument('--prc',type=Path,default=APP/'visorcheck.prc')
    decode=sub.add_parser('decode');decode.add_argument('pdb',type=Path)
    decode.add_argument('--session',required=True);decode.add_argument('--rom',type=Path,default=DEFAULT_ROM)
    for name in ('install','probe','run','collect'):
        child=sub.add_parser(name);child.add_argument('directory',type=Path)
        child.add_argument('--timeout',type=float,default=600)
        if name in ('install','probe'):
            child.add_argument('--transport',choices=('pilot-link','palm-sync'))
        if name=='run':
            child.add_argument('--kind',choices=KIND,default='rom')
            child.add_argument('--responder', choices=('cpu', 'pio'), default='cpu')
            child.add_argument('--marker', choices=MARKERS, default='drive')
            child.add_argument('--read-spacing', choices=READ_SPACING, default='tick')
            child.add_argument('--capture-profile', choices=CAPTURE_PROFILES, default='guarded')
            child.add_argument('--latency-full-rom', action='store_true',
                               help='full ROM through verified rx-first-v2 engine (requires --kind rom and CPU)')
            child.add_argument('--rp');child.add_argument('--read-timeout',type=float,default=180)
    args=parser.parse_args(argv)
    try:
        if getattr(args,'timeout',1)<=0 or getattr(args,'read_timeout',1)<=0:
            raise CollectionError('timeouts must be positive')
        if getattr(args, 'latency_full_rom', False) and (args.kind != 'rom' or args.responder != 'cpu'):
            raise CollectionError('--latency-full-rom requires --kind rom and --responder cpu')
        if args.command=='run' and KIND[args.kind] not in BUS_KINDS and (args.read_spacing!='tick' or args.capture_profile!='guarded'):
            raise CollectionError('synthetic results have no physical spacing/capture setting')
        return execute(args)
    except (CollectionError,OSError,ValueError,KeyError,struct.error) as exc:
        print('COLLECTION ERROR: '+str(exc),file=sys.stderr)
        if getattr(args,'directory',None) and (args.directory/'session.json').exists():
            with contextlib.suppress(Exception):
                atomic_json(args.directory/('collection-error-'+secrets.token_hex(4)+'.json'),
                            {'result':'COLLECTION ERROR','error':str(exc),'command':args.command})
        return 2
    except KeyboardInterrupt:
        print('INCOMPLETE: interrupted; evidence and pending run retained. Use collect to recover.',file=sys.stderr)
        return 130

if __name__=='__main__':
    sys.exit(main())
