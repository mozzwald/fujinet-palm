#!/usr/bin/env python3
"""Phase3 RAM timing receipts; serial palm-sync only, no RP commands."""
import argparse,json,struct,zlib
from pathlib import Path
import prism_collect as p
FIELDS=('magic','version','event','sequence','error','module','generation','source_db','runtime_db','base','size','original','requested','applied','restored','set_error','get_error','restore_error','guard_error','access_error','word0','word1','longword','flags','pc','code_base','code_size','heap','heap_flags','ram_heap_member','current_db','rom_version','hs_version','processor','oem_company','oem_device','seconds','stage','identity_error','storage_error','build','reason','code_crc','reserved1','reserved2','reserved3','reserved4','record_crc')
def decode_record(data,static):
 if len(data)!=192:raise ValueError('timing record must be192bytes')
 r=dict(zip(FIELDS,struct.unpack('>48I',data)))
 if r['magic']!=0x46544d33 or r['version']!=1 or r['event'] not in (0,1,2,3) or r['record_crc']!=zlib.crc32(data[:188]):raise ValueError('timing record magic/version/event/CRC')
 builds=static if isinstance(static,list) else [static]
 match=any(r['code_size']==s['text_bytes'] and r['code_crc']==int(s['code_crc32'],16) and r['pc']-r['code_base']==s['pc_marker_offset'] for s in builds)
 r['ram_execution_evidence']=bool(r['flags']&2 and r['ram_heap_member'] and not r['heap_flags']&1 and r['current_db'] and match and r['code_base']<=r['pc']<r['code_base']+r['code_size'])
 errors=('error','set_error','get_error','restore_error','guard_error','access_error','identity_error','storage_error')
 r['run_pass']=bool(r['event'] in (1,2) and r['ram_execution_evidence'] and r['flags']==14 and r['stage']==7 and not any(r[k] for k in errors) and r['base']%4==0 and r['word0']==0xfeed and r['word1']==0xbeef and r['longword']==0xfeedbeef and r['original']>0 and r['restored']==r['original'] and (r['event']==1 and r['applied']==r['original'] or r['event']==2 and r['original']==330 and r['requested']==360 and r['applied']>=360))
 return r
def decode_pdb(data,static):
 rows=[decode_record(b,static) for b in p.pdb_records(data,'FN Timing P3 Log',b'DATA',creator=b'FNTM')]
 key=lambda x:tuple(x[k] for k in ('module','generation','source_db','runtime_db','base','size','original'))
 baseline=set();complete=False
 for x in rows:
  if x['event']==2 and x['run_pass'] and key(x) in baseline:complete=True
  if x['event']==1 and x['run_pass']:baseline.add(key(x))
 return dict(records=rows,baseline_and_slower_complete=complete,physical_timing_verified=False)
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('mode',choices=('fetch','decode'));a.add_argument('--output',type=Path,required=True);a.add_argument('--static',type=Path,required=True);a.add_argument('--pdb',type=Path);a.add_argument('--previous-static',type=Path);a.add_argument('--timeout',type=float,default=900);args=a.parse_args();args.output.mkdir(parents=True,exist_ok=False)
 try:
  if args.mode=='fetch':p.transfer(args.output,args.output,'--fetch',['FN Timing P3 Log'],args.timeout,transport='palm-sync');source=args.output/'FN Timing P3 Log.pdb'
  else:
   if not args.pdb:raise ValueError('--pdb required')
   source=args.pdb
  builds=[json.loads(args.static.read_text())]
  if args.previous_static:builds.append(json.loads(args.previous_static.read_text()))
  r=decode_pdb(source.read_bytes(),builds);p.atomic_json(args.output/'report.json',r);print(json.dumps(r,indent=2))
  if not r['baseline_and_slower_complete']:a.exit(2,'Records preserved; baseline/slower acceptance incomplete.\n')
 except (OSError,ValueError,p.CollectionError) as e:a.exit(1,f'Timing collection incomplete: {e}\n')
if __name__=='__main__':main()
