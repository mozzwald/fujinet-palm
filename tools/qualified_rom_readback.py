#!/usr/bin/env python3
"""One version3 ROM readback after recorded paired qualification.

Separate explicit workflow; legacy synthetic/spacing gates are unchanged.
The user's one-read instruction authorizes this narrow replacement prerequisite.
Firmware, RAM PRC and previous results are never rewritten by this tool.
"""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import secrets
import struct
import subprocess
import time

import paired_collect as pairs
import prism_collect as p


def sha(data):
    return hashlib.sha256(data).hexdigest()


def resources(data):
    if len(data) < 80 or data[60:68] != b'applFNVD' or not int.from_bytes(data[32:34], 'big') & 1 or int.from_bytes(data[32:34], 'big') & 0x20:
        raise p.CollectionError('requires the non-reset FNVD RAM application')
    count = int.from_bytes(data[76:78], 'big')
    if len(data) < 80+10*count:
        raise p.CollectionError('truncated PRC resource table')
    entries = [struct.unpack_from('>4sHI', data, 78+10*i) for i in range(count)]
    offsets = [e[2] for e in entries] + [len(data)]
    if not count or offsets != sorted(offsets) or offsets[0] < 78+10*count or offsets[-2] >= len(data):
        raise p.CollectionError('invalid PRC resources')
    result = {(typ, rid):data[start:offsets[i+1]] for i,(typ,rid,start) in enumerate(entries)}
    if len(result) != count:
        raise p.CollectionError('duplicate PRC resource')
    return result


def qualification(source, firmware):
    source = source.resolve()
    state = json.loads((source/'session.json').read_text())
    rom = (source/'reference.rom').read_bytes()
    prc = (source/'paired-checker.prc').read_bytes()
    if (state.get('profile') != 'paired-move-l-v1' or state.get('transport') != 'palm-sync' or
            not state.get('installed') or not state.get('transport_verified') or state.get('pending') or
            state.get('active_firmware_sha256') != firmware or len(firmware) != 64 or
            state['rom_sha256'] != sha(rom) or state['prc_sha256'] != sha(prc)):
        raise p.CollectionError('paired qualification identity, transport or pending state mismatch')
    resources(prc)
    pairs.configure()
    decoded = pairs.decode_pdb((source/state['previous_pdb']).read_bytes(), state['session'], rom)
    proof = []
    for offset in pairs.patterns(rom).values():
        for count in (4,100,10000):
            candidates = [v for v in state['validation'] if v['offset']==offset and v['count']==count and
                          v.get('qualified_pass') and v.get('firmware_sha256')==firmware]
            if not candidates:
                raise p.CollectionError('both patterns must pass4/100/10000 on the frozen firmware')
            sequence = candidates[-1]['sequence']
            raw = decoded[sequence]['final']
            locations = list(source.glob(f'run-{sequence:06d}-*/rp-final.json'))
            if len(locations) != 1:
                raise p.CollectionError('missing/ambiguous paired counter evidence')
            final = json.loads(locations[0].read_text())
            armed = json.loads(locations[0].with_name('rp-armed.json').read_text())
            config = json.loads(locations[0].with_name('rp-config.json').read_text())
            if (raw['status'] != 1 or raw['offset'] != offset or raw['requested'] != count or raw['completed'] != count or
                    raw['first_bad'] or raw['second_bad'] or final['run'] != armed['run'] or
                    any(final[k] != 2*count for k in ('requests','posted','completed','attempted')) or
                    any(final[k] for k in ('cancelled','empty_ends','active','arm_pending')) or
                    final['stopped'] != 1 or final.get('counters_final') != 1 or
                    config.get('engine') != 'continuous-v3' or config.get('marker') != 'preload' or
                    config.get('response') != 'cpu-rom' or config.get('words') != len(rom)//2):
                raise p.CollectionError('paired result/counter/engine qualification failed')
            proof.append(dict(sequence=sequence, offset=offset, count=count, counters_sha256=sha(locations[0].read_bytes())))
    return rom, prc, dict(source=str(source), firmware_sha256=firmware, rom_sha256=sha(rom),
                          prc_sha256=sha(prc), gates=proof)


def prepare(args):
    if not args.authorization.strip():
        raise p.CollectionError('record the explicit one-read instruction')
    rom,prc,proof = qualification(args.paired_session,args.firmware_sha256)
    p.configure_profile('romprobe')
    directory = args.directory.resolve()
    directory.mkdir(parents=True,exist_ok=False)
    (directory/'reference.rom').write_bytes(rom)
    (directory/'visorcheck.prc').write_bytes(prc)
    session=secrets.token_hex(16)
    (directory/'ROMProbeControl.pdb').write_bytes(p.make_pdb('ROMProbeControl',[p.config_record(session,rom)],b'VCTL'))
    proof['authorization']=args.authorization
    proof['scope']='one full-ROM read after paired qualification; no fabricated synthetic/spacing history'
    p.atomic_json(directory/'qualification.json',proof)
    p.atomic_json(directory/'session.json',dict(version=1,profile='romprobe',workflow='qualified-persistent-once',
        session=session,rom_sha256=sha(rom),prc_sha256=sha(prc),firmware_sha256=args.firmware_sha256,
        qualification_sha256=sha((directory/'qualification.json').read_bytes()),last_sequence=0,pending=None,
        installed=False,transport_verified=False,transport='palm-sync',validation=[]))
    print('Prepared one-read session; firmware and installed PRC unchanged.')


def load(directory):
    p.configure_profile('romprobe')
    directory,state,rom=p.load_session(directory)
    if state.get('workflow')!='qualified-persistent-once' or state.get('transport')!='palm-sync':
        raise p.CollectionError('wrong one-read workflow')
    if sha((directory/'qualification.json').read_bytes()) != state['qualification_sha256'] or sha((directory/'visorcheck.prc').read_bytes()) != state['prc_sha256']:
        raise p.CollectionError('prepared qualification or PRC changed')
    proof=json.loads((directory/'qualification.json').read_text())
    if any(proof[key]!=state[key] for key in ('firmware_sha256','rom_sha256','prc_sha256')):
        raise p.CollectionError('qualification binding changed')
    return directory,state,rom


def connect(directory,state,timeout):
    if state['pending'] or state['last_sequence']:
        raise p.CollectionError('cannot reconnect a pending/completed one-read session')
    output=directory/('control-roundtrip-'+secrets.token_hex(4));output.mkdir()
    port=p.wait_palm(timeout)
    prc=(directory/'visorcheck.prc').read_bytes()
    command=['node',str(Path(__file__).with_name('control_roundtrip.cjs')),port,
             str(directory/'ROMProbeControl.pdb'),prc[:32].split(b'\0')[0].decode(),str(output)]
    p.atomic_json(output/'command.json',command)
    with (output/'transport.log').open('xb') as log:
        proc=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
    if proc.returncode:
        raise p.CollectionError('control roundtrip failed; raw output retained')
    actual=(output/'installed-app.prc').read_bytes()
    if actual[:32] != prc[:32] or resources(actual) != resources(prc):
        raise p.CollectionError('installed RAM app resources differ from qualified PRC')
    if p.pdb_records((output/'ROMProbeControl.pdb').read_bytes(),'ROMProbeControl',b'VCTL') != [p.config_record(state['session'],(directory/'reference.rom').read_bytes())]:
        raise p.CollectionError('full-ROM control roundtrip mismatch')
    state.update(installed=True,transport_verified=True)
    p.atomic_json(directory/'session.json',state)
    p.atomic_json(output/'verification.json',dict(installed_PRC_resources_match=True,control_roundtrip=True,
                                               PRC_installed=False,reset_requested=False))
    print('Installed RAM app verified; control uploaded and round-tripped. No ROM read or reset.')


def run(directory,state,rom,args):
    if not state['transport_verified'] or state['pending'] or state['last_sequence']:
        raise p.CollectionError('requires verified control and exactly one unused readback')
    output=directory/('run-000001-'+secrets.token_hex(4));output.mkdir()
    rp=p.ContinuousRP(args.rp,output/'rp-console.log');final=None
    try:
        before,config=rp.identify_continuous(rom,'cpu-rom',len(rom)//2,marker='preload',engine='persistent')
        p.atomic_json(output/'rp-before.json',before);p.atomic_json(output/'rp-config.json',config)
        state['pending']=dict(sequence=1,kind='rom',directory=output.name,read_spacing='none',capture_profile='burst64',
                              engine='persistent',firmware_sha256=state['firmware_sha256'])
        p.atomic_json(directory/'session.json',state)
        armed=rp.arm(before,len(rom)//2,ready_timeout=60);p.atomic_json(output/'rp-armed.json',armed)
        print(f'ARMED RP{armed["run"]}: one full ROM, {len(rom)//2} words. Run/Read ONCE with None + Burst64, then saved tone and HotSync. No UART polling.',flush=True)
        end=time.monotonic()+args.read_timeout
        while not p.find_palm():
            if time.monotonic()>end:raise p.CollectionError('read wait timed out; preserve Palm result')
            time.sleep(.1)
        final=rp.stop();p.atomic_json(output/'rp-final.json',final)
        return collect(directory,state,rom,args.timeout)
    finally:
        try:
            if final is None:p.atomic_json(output/'rp-final.json',rp.stop())
        finally:rp.close()


def collect(directory,state,rom,timeout):
    if not state['pending']:raise p.CollectionError('no pending readback')
    output=directory/state['pending']['directory']
    code=p.collect(directory,state,rom,timeout)
    if code==0:
        report=json.loads((output/'report.json').read_text())
        if report.get('payload_sha256')!=sha(rom):raise p.CollectionError('payload SHA mismatch')
        download=max(output.glob('download-*/ROMProbeResults.pdb'),key=lambda path:path.stat().st_mtime_ns)
        records=p.pdb_records(download.read_bytes())
        finals=[raw for raw in records if (d:=p.decode_record(raw,rom))['session']==state['session'] and d['sequence']==1 and d['status']==1]
        if len(finals)!=1:raise p.CollectionError('ambiguous final payload')
        header=p.decode_record(finals[0],rom)['header_size'];payload=finals[0][header:header+len(rom)]
        if payload!=rom:raise p.CollectionError('payload byte comparison failed')
        (output/'readback.rom').write_bytes(payload)
        report['firmware_sha256']=state['firmware_sha256']
        p.atomic_json(output/'report.json',report)
        print(f'Full ROM independently verified: {len(rom)} bytes CRC={p.crc(payload):08X} SHA256={sha(payload)}',flush=True)
    return code


def readback_gate(directory,rom,firmware):
    directory,state,reference=load(directory)
    if state['pending'] or state['last_sequence']!=1 or reference!=rom or state['firmware_sha256']!=firmware:
        raise p.CollectionError('complete matching readback required before recognition')
    reports=list(directory.glob('run-000001-*/report.json'))
    if len(reports)!=1:
        raise p.CollectionError('missing/ambiguous readback report')
    result=json.loads(reports[0].read_text())
    final=result.get('rp',{})
    if (result['result']!='PASS' or result.get('firmware_sha256')!=firmware or
            result.get('payload_sha256')!=sha(rom) or reports[0].with_name('readback.rom').read_bytes()!=rom or
            result.get('completed')!=len(rom) or result.get('expected_size')!=len(rom) or result.get('mismatch_count')!=0 or
            any(final.get(k)!=len(rom)//2 for k in ('requests','posted','completed','attempted')) or
            any(final.get(k)!=0 for k in ('cancelled','empty_ends','active','arm_pending')) or
            final.get('stopped')!=1 or final.get('counters_final')!=1 or
            result.get('rp_config',{}).get('engine')!='continuous-v3' or
            result.get('rp_config',{}).get('marker')!='preload'):
        raise p.CollectionError('readback payload/engine verification failed')
    return dict(session=str(directory), firmware_sha256=firmware, rom_sha256=sha(rom),
                report_sha256=sha(reports[0].read_bytes()))


def recover_startup(directory,state):
    pending=state['pending']
    if not pending or state['last_sequence'] or pending['sequence']!=1:
        raise p.CollectionError('not a recoverable unused startup')
    output=directory/pending['directory']
    if (output/'rp-armed.json').exists() or (output/'report.json').exists():
        raise p.CollectionError('an armed/read trial must be collected, never abandoned')
    final=json.loads((output/'rp-final.json').read_text())
    if (final.get('stopped')!=1 or any(final.get(k)!=0 for k in
            ('active','arm_pending','requests','posted','completed','attempted','cancelled','empty_ends'))):
        raise p.CollectionError('startup has active or nonzero transaction evidence')
    p.atomic_json(output/'startup-aborted.json',dict(test_started=False,read_cue_given=False,
        reason='Host arm confirmation did not observe active; stopped with zero requests. Preserve this attempt.'))
    state.setdefault('aborted_startups',[]).append(pending['directory'])
    state['pending']=None;p.atomic_json(directory/'session.json',state)
    print('Zero-request startup preserved; the single ROM read remains unused.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    prep=sub.add_parser('prepare');prep.add_argument('directory',type=Path);prep.add_argument('--paired-session',type=Path,required=True)
    prep.add_argument('--firmware-sha256',required=True);prep.add_argument('--authorization',required=True)
    for name in ('connect','run','collect','recover-startup'):
        child=sub.add_parser(name);child.add_argument('directory',type=Path);child.add_argument('--timeout',type=float,default=1800)
        if name=='run':child.add_argument('--rp',default='/dev/ttyACM0');child.add_argument('--read-timeout',type=float,default=7200)
    args=parser.parse_args()
    if args.command=='prepare':prepare(args);return 0
    directory,state,rom=load(args.directory)
    with (directory/'session.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        if args.command=='connect':connect(directory,state,args.timeout);return 0
        if args.command=='recover-startup':recover_startup(directory,state);return 0
        if args.command=='collect':return collect(directory,state,rom,args.timeout)
        return run(directory,state,rom,args)


if __name__=='__main__':
    try:raise SystemExit(main())
    except (OSError,ValueError,KeyError,p.CollectionError,subprocess.TimeoutExpired) as error:
        print('INCOMPLETE: '+str(error),flush=True);raise SystemExit(2)
