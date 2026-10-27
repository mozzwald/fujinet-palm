#!/usr/bin/env python3
"""Decode or fetch Phase2 RAM proof records using serial palm-sync only.

Never opens the RP2350, uploads Setup PRCs, triggers reset or launches an app.
"""
import argparse
import json
from pathlib import Path
import struct
import prism_collect as p
FIELDS=('magic','format','event','generation','module','is_reset','error','source_db','runtime_db',
 'setup_pc','setup_base','setup_size','setup_heap','setup_heapflags','setup_card','run_pc','run_base',
 'run_size','run_heap','run_heapflags','run_card','loop_start','loop_end','loop_result','receipt_active',
 'context','stage','aux_error','source_crc','run_crc','seconds','ram_heap_checks')

def decode_record(data,static):
    if len(data)!=128:raise ValueError('record must be128bytes')
    r=dict(zip(FIELDS,struct.unpack('>32I',data)))
    if r['magic']!=0x46525032 or r['format']!=1 or r['event'] not in range(1,6):
        raise ValueError('wrong RAM proof magic/version/event')
    crc=int(static['code_crc32'],16)
    def within(pc,base,size):return base>0 and size>0 and base+size<=0xffffffff and base<=pc<base+size
    r['setup_ram_evidence']=bool(r['ram_heap_checks']&1 and r['setup_card']==0 and not r['setup_heapflags']&1 and
        r['source_db'] and r['source_crc']==crc and r['setup_size']==static['text_bytes'] and within(r['setup_pc'],r['setup_base'],r['setup_size']) and
        r['setup_pc']-r['setup_base']==static['pc_marker_offset'])
    r['installation_evidence']=bool(r['event']==2 and r['setup_ram_evidence'] and r['runtime_db'] and
        r['stage']==7 and not r['error'] and r['receipt_active'])
    r['runtime_ram_evidence']=bool(r['event'] in (4,5) and r['setup_ram_evidence'] and r['ram_heap_checks']&2 and
        r['run_card']==0 and not r['run_heapflags']&1 and r['run_crc']==crc and r['context']==r['runtime_db'] and r['run_size']==static['text_bytes'] and not r['error'] and
        r['stage']==7 and within(r['run_pc'],r['run_base'],r['run_size']) and
        r['run_pc']-r['run_base']==static['pc_marker_offset'] and
        r['loop_start']-r['run_base']==static['loop_start_offset'] and
        r['loop_end']-r['run_base']==static['loop_end_offset'] and
        r['run_base']<=r['loop_start']<r['loop_end']<=r['run_base']+r['run_size'])
    r['loop_evidence']=bool(r['event']==5 and r['runtime_ram_evidence'] and r['loop_result']==0x52504c50)
    return r

def decode_pdb(data,static):
    rows=[decode_record(b,static) for b in p.pdb_records(data,'FN RAM Proof',b'DATA',creator=b'FNRA')]
    installs={(r['generation'],r['runtime_db'],r['source_db']) for r in rows if r['installation_evidence']}
    return dict(records=rows,installation_generations=sorted({i[0] for i in installs}),
        matching_install_and_runtime=any(r['runtime_ram_evidence'] and (r['generation'],r['runtime_db'],r['source_db']) in installs for r in rows),
        physical_scope_verified=False,physical_lifecycle_verified=False)

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('mode',choices=('decode','fetch'));a.add_argument('--pdb',type=Path)
    a.add_argument('--static',type=Path,required=True);a.add_argument('--output',type=Path,required=True)
    a.add_argument('--timeout',type=float,default=180);args=a.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    try:
        if args.mode=='fetch':
            print('Waiting for manual HotSync; serial palm-sync only.',flush=True)
            p.transfer(args.output,args.output,'--fetch',['FN RAM Proof'],args.timeout,transport='palm-sync')
            source=args.output/'FN RAM Proof.pdb'
        else:
            if not args.pdb:raise ValueError('--pdb is required for decode')
            source=args.pdb
        report=decode_pdb(source.read_bytes(),json.loads(args.static.read_text()))
        p.atomic_json(args.output/'report.json',report);print(json.dumps(report,indent=2))
        if not report['matching_install_and_runtime']:a.exit(2,'Collected records; matching installation/RAM execution evidence is incomplete.\n')
    except (OSError,ValueError,p.CollectionError) as e:a.exit(1,f'RAM proof incomplete: {e}\n')
if __name__=='__main__':main()
