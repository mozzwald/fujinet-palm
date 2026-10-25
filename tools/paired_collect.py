#!/usr/bin/env python3
"""Bounded MOVE.L results, using the established serial palm-sync/PDB/RP tools.

Never flashes, changes slot timing, resets PalmOS or attempts recognition.
No UART polling between arming and the user's post-test HotSync.
"""
import argparse
import csv
import fcntl
import hashlib
import json
from pathlib import Path
import secrets
import struct
import time
import prism_collect as p

DB='ROMPairResults'
CONTROL='ROMPairControl'
HEADER=160
COUNTS=(4,100,10000)
WORD_MODE=False
NOP_COUNTS=(0,1,2,4,8,16,32,64)
FIELDS=('sequence status offset expected requested completed first_bad second_bad long_bad '
        'base size before after reason error started ended payload_crc reference_crc '
        'app_version length header_size opcodes_per_value interpair_ticks reserved1 reserved2').split()

def configure():
    global DB,CONTROL
    p.configure_profile('romprobe')
    DB,CONTROL=('ROMWordResults','ROMWordControl') if WORD_MODE else ('ROMPairResults','ROMPairControl')
    p.DB,p.CONTROL_DB=DB,CONTROL
    if WORD_MODE:p.DB_CREATOR=b'FNVE'

def patterns(rom):
    if len(rom)<12 or rom[8:12]!=bytes.fromhex('feedbeef'):
        raise p.CollectionError('reference lacks FEEDBEEF at offset8')
    # Earliest maximum, matching the RAM application's selection.
    offset=max(range(0,len(rom)-3,4),key=lambda i:
               (int.from_bytes(rom[i:i+2],'big')^int.from_bytes(rom[i+2:i+4],'big')).bit_count())
    return {'signature':8,'contrast':offset}

def decode_record(raw,rom):
    magic=b'VPW1' if WORD_MODE else b'VPL1'
    if len(raw)<HEADER+4 or raw[:8]!=magic+b'\0\0\0\1':
        raise p.CollectionError('unknown/truncated paired record')
    d=dict(zip(FIELDS,struct.unpack_from('>26I',raw,56)))
    d['session']=raw[8:24].hex();d['rom_sha256']=raw[24:56].hex()
    if (d['length']!=len(raw) or d['header_size']!=HEADER or
        len(raw)!=HEADER+4*d['completed']+4 or
        p.crc(raw[:-4])!=struct.unpack_from('>I',raw,len(raw)-4)[0]):
        raise p.CollectionError('paired record length/CRC mismatch')
    metadata=(d['app_version']==0x800 and d['opcodes_per_value']==2 and
              d['reserved1']==1 and d['reserved2'] in NOP_COUNTS) if WORD_MODE else (
              d['app_version']==0x700 and d['opcodes_per_value']==1 and
              d['reserved1']==0 and d['reserved2']==0)
    if (not d['sequence'] or d['status'] not in p.STATUS or d['requested'] not in ((4,) if WORD_MODE else COUNTS) or
        d['completed']>d['requested'] or not metadata or d['interpair_ticks']!=1 or
        d['offset']%4 or d['offset'] not in patterns(rom).values() or
        d['rom_sha256']!=hashlib.sha256(rom).hexdigest() or d['reference_crc']!=p.crc(rom) or
        d['expected']!=int.from_bytes(rom[d['offset']:d['offset']+4],'big')):
        raise p.CollectionError('paired test/reference metadata mismatch')
    payload=raw[HEADER:-4]
    if p.crc(payload)!=d['payload_crc']:
        raise p.CollectionError('paired payload CRC mismatch')
    values=[v[0] for v in struct.iter_unpack('>I',payload)]
    expected=d['expected'];first=sum(v>>16!=expected>>16 for v in values)
    second=sum((v&0xffff)!=(expected&0xffff) for v in values)
    bad=sum(v!=expected for v in values)
    if (first,second,bad)!=(d['first_bad'],d['second_bad'],d['long_bad']):
        raise p.CollectionError('paired halfword counts disagree with complete payload')
    if d['status']==0 and (d['completed'] or d['ended'] or d['after'] or d['error'] or d['reason']):
        raise p.CollectionError('invalid paired STARTED marker')
    if d['status'] in (1,2) and (d['completed']!=d['requested'] or
        (d['status']==1)!=(bad==0) or d['reason'] or d['error'] or
        not d['before'] or d['after']!=d['before']):
        raise p.CollectionError('invalid paired PASS/FAIL or timing claim')
    d.update(first_correct=len(values)-first,second_correct=len(values)-second,
             result=p.STATUS[d['status']],values_hex=[f'{v:08X}' for v in values],
             instruction_mode='two-MOVE.W' if WORD_MODE else 'one-MOVE.L',
             nop_count=d['reserved2'] if WORD_MODE else 0)
    d['mismatches']=[dict(index=i,actual=f'{v:08X}',expected=f'{expected:08X}',
        first_actual=f'{v>>16:04X}',first_expected=f'{expected>>16:04X}',
        second_actual=f'{v&65535:04X}',second_expected=f'{expected&65535:04X}',
        first_correct=v>>16==expected>>16,second_correct=(v&65535)==(expected&65535))
        for i,v in enumerate(values) if v!=expected]
    return d

def decode_pdb(data,session,rom):
    runs={}
    for raw in p.pdb_records(data,DB):
        # Validate the envelope of every record; only this session uses this ROM.
        if len(raw)<HEADER+4 or raw[:8]!=(b'VPW1' if WORD_MODE else b'VPL1')+b'\0\0\0\1' or p.crc(raw[:-4])!=int.from_bytes(raw[-4:],'big'):
            raise p.CollectionError('corrupt paired database history')
        if raw[8:24].hex()!=session:continue
        d=decode_record(raw,rom);key='start' if d['status']==0 else 'final'
        pair=runs.setdefault(d['sequence'],{})
        if key in pair:raise p.CollectionError('duplicate paired result/STARTED marker')
        pair[key]=d
    for pair in runs.values():
        if 'start' not in pair:raise p.CollectionError('paired final without STARTED')
        if 'final' in pair and any(pair['start'][k]!=pair['final'][k] for k in
            ('offset','expected','requested','started','base','size','before','app_version','reserved1','reserved2')):
            raise p.CollectionError('paired run markers disagree')
    return runs

def report(output,d,armed,final):
    result=dict(d,rp_armed=armed,rp_final=final)
    result['expected_halfword_transactions']=2*d['completed']
    wanted=2*d['requested']
    ownership=(final['run']==armed['run'] and final['stopped']==1 and not final['active'] and
               not final['arm_pending'] and final['limit']==0 and final['continuous']==1)
    counters=all(final[k]==wanted for k in ('requests','posted','completed','attempted')) and not any(
        final[k] for k in ('cancelled','empty_ends'))
    result['counter_match']=bool(ownership and counters)
    # Missing requests are diagnostic evidence, not a reason to discard bytes.
    result['qualified_pass']=d['status']==1 and result['counter_match']
    result['scope_timing']='Await scope/logic analysis; no numerical setup/hold margin or electrical Hi-Z claim.'
    result['localization']='Counter deficits can precede CPU reply publication; exact stage needs captured pair/marker correlation.'
    p.atomic_json(output/'report.json',result)
    fields=('index','actual','expected','first_actual','first_expected','second_actual',
            'second_expected','first_correct','second_correct')
    with (output/'mismatches.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader();writer.writerows(d['mismatches'])
    lines=['# '+('Two-MOVE.W NOP-spacing diagnostic' if WORD_MODE else 'Paired MOVE.L diagnostic'),'',
        '| Pairs | First correct / bad | Second correct / bad | Full bad | Requests | Posted | Completed | Enable commands | Cancels |',
        '| --- | --- | --- | --- | --- | --- | --- | --- | --- |',
        f"| {d['completed']}/{d['requested']} | {d['first_correct']} / {d['first_bad']} | {d['second_correct']} / {d['second_bad']} | {d['long_bad']} | {final['requests']} | {final['posted']} | {final['completed']} | {final['attempted']} | {final['cancelled']} |",'',
        f"Offset {d['offset']:04X}, expected {d['expected']:08X}. Palm result {d['result']}; exact responder counts: {result['counter_match']}.",
        f"Access before/after: {d['before']}/{d['after']} ns; no setters used. All returned values are in report.json and original PDB.",
        (f"Two explicit MOVE.W loads, {d['nop_count']} unrolled NOPs between loads. " if WORD_MODE else '')+
        'One tick between pairs; physical intra-pair timing requires capture. No recognition or execution tested.',
        f"GPIO39 marker={d.get('rp_config',{}).get('marker','drive')}; attempted counts unmasked-enable outcomes, not valid data or physical Hi-Z.",'',
        '| Index | Full actual | Full expected | First actual | Second actual |','| --- | --- | --- | --- | --- |']
    lines += [f"| {m['index']} | {m['actual']} | {m['expected']} | {m['first_actual']} | {m['second_actual']} |" for m in d['mismatches'][:16]]
    lines += ['','All mismatches, without truncation: mismatches.csv. Physical timing and localization remain pending.']
    (output/'report.md').write_text('\n'.join(lines)+'\n')
    return result

def load(directory):
    directory=directory.resolve();state=json.loads((directory/'session.json').read_text())
    rom=(directory/'reference.rom').read_bytes()
    if state['profile']!=('paired-move-w-v1' if WORD_MODE else 'paired-move-l-v1') or state['transport']!='palm-sync' or state['rom_sha256']!=hashlib.sha256(rom).hexdigest():
        raise p.CollectionError('paired session identity mismatch')
    return directory,state,rom

def check_escalation(state,offset,count):
    if count==4:return
    previous=4 if count==100 else 100
    active=state.get('active_firmware_sha256')
    if not any(v['offset']==offset and v['count']==previous and v['qualified_pass'] and
               (not active or v.get('firmware_sha256')==active)
               for v in state['validation']):
        raise p.CollectionError(f'first pass {previous} pairs at this pattern, including exact responder counters')

def collect(directory,state,rom,timeout):
    pending=state['pending']
    if not pending:raise p.CollectionError('no pending paired run')
    output=directory/pending['directory'];download=output/('download-'+secrets.token_hex(4));download.mkdir()
    p.transfer(directory,download,'--fetch',[DB],timeout,transport='palm-sync')
    data=(download/(DB+'.pdb')).read_bytes();runs=decode_pdb(data,state['session'],rom)
    seq=pending['sequence']
    if max(runs,default=0)!=seq or seq not in runs or 'final' not in runs[seq]:
        raise p.CollectionError('missing/stale/extra paired run; raw download retained')
    if state.get('previous_pdb'):
        previous=p.pdb_records((directory/state['previous_pdb']).read_bytes(),DB)
        if p.pdb_records(data,DB)[:len(previous)]!=previous:
            raise p.CollectionError('previous paired records changed or vanished')
    d=runs[seq]['final']
    d['firmware_sha256']=pending.get('firmware_sha256',state.get('firmware_sha256'))
    d['rp_config']=json.loads((output/'rp-config.json').read_text())
    if d['offset']!=pending['offset'] or d['requested']!=pending['count']:
        raise p.CollectionError('Palm pattern/count differs from armed host run')
    if WORD_MODE and d['nop_count']!=pending['nops']:
        raise p.CollectionError('Palm NOP count differs from armed host run; raw result preserved')
    armed=json.loads((output/'rp-armed.json').read_text());final=json.loads((output/'rp-final.json').read_text())
    result=report(output,d,armed,final)
    state['last_sequence']=seq;state['pending']=None
    state['previous_pdb']=str((download/(DB+'.pdb')).relative_to(directory))
    state.setdefault('validation',[]).append(dict(sequence=seq,offset=d['offset'],count=d['requested'],
            qualified_pass=result['qualified_pass'],result=d['result'],firmware_sha256=d['firmware_sha256'],
            instruction_mode=d['instruction_mode'],nop_count=d['nop_count']))
    p.atomic_json(directory/'session.json',state)
    print(f"{d['result']}: first bad={d['first_bad']}, second bad={d['second_bad']}, requests={final['requests']}/{2*d['requested']}; {output/'report.md'}",flush=True)
    return 0 if result['qualified_pass'] else 1

def main():
    configure();parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    setup=sub.add_parser('setup');setup.add_argument('directory',type=Path)
    setup.add_argument('--rom',type=Path,required=True);setup.add_argument('--prc',type=Path,required=True)
    for cmd in ('install','probe','run','collect'):
        sp=sub.add_parser(cmd);sp.add_argument('directory',type=Path);sp.add_argument('--timeout',type=float,default=600)
        if cmd=='run':
            sp.add_argument('--pattern',choices=('signature','contrast'),default='signature')
            sp.add_argument('--count',type=int,choices=(4,) if WORD_MODE else COUNTS,default=4)
            sp.add_argument('--marker',choices=('drive','preload'),default='drive')
            sp.add_argument('--engine',choices=('rearm','persistent'),default='rearm')
            if WORD_MODE:
                sp.add_argument('--nops',type=int,choices=NOP_COUNTS,default=0)
            sp.add_argument('--rp',default='/dev/ttyACM0');sp.add_argument('--read-timeout',type=float,default=600)
    args=parser.parse_args()
    if args.command=='setup':
        directory=args.directory.resolve();rom=args.rom.read_bytes();prc=args.prc.read_bytes();patterns(rom)
        if len(prc)<78 or prc[60:68]!=b'appl'+(b'FNVE' if WORD_MODE else b'FNVD') or int.from_bytes(prc[32:34],'big')&0x20:
            raise p.CollectionError('requires matching distinct RAM diagnostic without reset-after-install')
        directory.mkdir(parents=True,exist_ok=False);session=secrets.token_hex(16)
        (directory/'reference.rom').write_bytes(rom);(directory/('wordpair-checker.prc' if WORD_MODE else 'paired-checker.prc')).write_bytes(prc)
        (directory/(CONTROL+'.pdb')).write_bytes(p.make_pdb(CONTROL,[p.config_record(session,rom)],b'VCTL'))
        p.atomic_json(directory/'session.json',dict(version=1,profile='paired-move-w-v1' if WORD_MODE else 'paired-move-l-v1',transport='palm-sync',
            session=session,rom_sha256=hashlib.sha256(rom).hexdigest(),prc_sha256=hashlib.sha256(prc).hexdigest(),
            installed=False,transport_verified=False,last_sequence=0,pending=None,validation=[]))
        print('Prepared paired session; no hardware accessed.');return 0
    directory,state,rom=load(args.directory)
    with (directory/'session.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        if args.command in ('install','probe'):
            if state['pending']:raise p.CollectionError('run pending; do not install/probe')
            output=directory/(args.command+'-'+secrets.token_hex(4));output.mkdir()
            if args.command=='install':
                prc=directory/('wordpair-checker.prc' if WORD_MODE else 'paired-checker.prc')
                if hashlib.sha256(prc.read_bytes()).hexdigest()!=state['prc_sha256']:
                    raise p.CollectionError('prepared PRC changed')
                p.transfer(directory,output,'--install',[prc,directory/(CONTROL+'.pdb')],args.timeout,transport='palm-sync')
                state['installed']=True;state['transport_verified']=False
            else:
                if not state['installed']:raise p.CollectionError('install first')
                p.transfer(directory,output,'--fetch',[CONTROL],args.timeout,transport='palm-sync')
                if p.pdb_records((output/(CONTROL+'.pdb')).read_bytes(),CONTROL,b'VCTL')!=[p.config_record(state['session'],rom)]:
                    raise p.CollectionError('paired control round-trip mismatch')
                state['transport_verified']=True
            p.atomic_json(directory/'session.json',state);print(args.command+': successful');return 0
        if args.command=='collect':return collect(directory,state,rom,args.timeout)
        if not state['transport_verified']:raise p.CollectionError('install and probe control first')
        if state['pending']:raise p.CollectionError('uncollected run pending; use collect')
        offset=patterns(rom)[args.pattern]
        check_escalation(state,offset,args.count)
        seq=state['last_sequence']+1;output=directory/f'run-{seq:06d}-{secrets.token_hex(4)}';output.mkdir()
        pending=dict(sequence=seq,count=args.count,offset=offset,pattern=args.pattern,directory=output.name,
                     firmware_sha256=state.get('active_firmware_sha256',state.get('firmware_sha256')),
                     marker=args.marker,engine=args.engine)
        if WORD_MODE:pending['nops']=args.nops
        rp=p.ContinuousRP(args.rp,output/'rp-console.log');final=None
        try:
            before,config=rp.identify_continuous(rom,'cpu-rom',len(rom)//2,marker=args.marker,engine=args.engine)
            p.atomic_json(output/'rp-before.json',before);p.atomic_json(output/'rp-config.json',config)
            state['pending']=pending;p.atomic_json(directory/'session.json',state)
            armed=rp.arm(before,2*args.count);p.atomic_json(output/'rp-armed.json',armed)
            spacing=f", NOPs={args.nops}" if WORD_MODE else ''
            print(f"ARMED continuous RP{armed['run']}; count={args.count}, offset={offset:04X}{spacing}, then Run pair/Read ONCE. Save tone then HotSync. No UART polling until HotSync.",flush=True)
            # A tick separates pairs; there are no console requests/logging
            # during accesses. Only wait for the existing USB HotSync signal.
            deadline=time.monotonic()+args.read_timeout
            while not p.find_palm():
                if time.monotonic()>deadline:raise p.CollectionError('paired read timeout; results remain on Palm')
                time.sleep(.1)
            final=rp.stop();p.atomic_json(output/'rp-final.json',final)
            return collect(directory,state,rom,args.timeout)
        finally:
            try:
                if final is None:p.atomic_json(output/'rp-final.json',rp.stop())
            finally:rp.close()

if __name__=='__main__':
    try:raise SystemExit(main())
    except (OSError,ValueError,p.CollectionError) as error:
        print('INCOMPLETE: '+str(error),flush=True);raise SystemExit(2)
