#!/usr/bin/env python3
"""Serial palm-sync Stage A receipt retrieval; no RP arm/stop/flash commands."""
import argparse,json,struct,zlib
from pathlib import Path
import prism_collect as p
FIELDS=('magic','version','sequence','error','module','generation','cs0','cs_size','cs1','access_ns','pc','code_base','code_size','heap','heap_flags','ram_heap_member','current_db','code_crc','id','abi','capabilities','status','initial_ack','final_ack','expected_ack','accepted_iterations','polls','rom_word0','rom_word1','reserved2','seconds','record_crc')
def decode_record(data,static):
 if len(data)!=128:raise ValueError('Stage A record size')
 r=dict(zip(FIELDS,struct.unpack('>32I',data)))
 if r['magic']!=0x564d4131 or r['version']!=1 or r['record_crc']!=zlib.crc32(data[:124]):raise ValueError('Stage A identity/CRC')
 r['ram_execution_evidence']=bool(r['ram_heap_member'] and not r['heap_flags']&1 and r['current_db'] and r['code_size']==static['text_bytes'] and r['code_crc']==int(static['code_crc32'],16) and r['pc']-r['code_base']==static['pc_marker_offset'] and r['code_base']<=r['pc']<r['code_base']+r['code_size'])
 r['run_pass']=bool(r['ram_execution_evidence'] and not r['error'] and r['access_ns']==330 and r['cs1']==r['cs0']+r['cs_size'] and r['cs_size']==0x1000000 and not r['cs0']&0xffffff and not r['cs1']&0xffffff and r['cs1']<=0xffffffff and not r['cs1']&1 and r['id']==0x464e and r['abi']==0x100 and r['capabilities']==1 and r['status']==1 and r['accepted_iterations']==4 and r['final_ack']==r['expected_ack']==r['initial_ack']+4 and 4<=r['polls']<=256 and r['rom_word0']==0xfeed and r['rom_word1']==0xbeef)
 return r
def decode_pdb(data,static):
 return dict(records=[decode_record(b,static) for b in p.pdb_records(data,static.get('log_name','FN CS1 Stage A Log'),b'DATA',creator=static.get('runtime_creator','FNMA').encode('ascii'))],physical_cs1_qualified=False)
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('mode',choices=('fetch','decode'));a.add_argument('--static',type=Path,required=True);a.add_argument('--output',type=Path,required=True);a.add_argument('--pdb',type=Path);a.add_argument('--timeout',type=float,default=900);x=a.parse_args();x.output.mkdir(parents=True,exist_ok=False)
 try:
  if x.mode=='fetch':
   static=json.loads(x.static.read_text());name=static.get('log_name','FN CS1 Stage A Log');p.transfer(x.output,x.output,'--fetch',[name],x.timeout,transport='palm-sync');source=x.output/(name+'.pdb')
  else:
   if not x.pdb:raise ValueError('--pdb required')
   source=x.pdb
  r=decode_pdb(source.read_bytes(),json.loads(x.static.read_text()));p.atomic_json(x.output/'report.json',r);print(json.dumps(r,indent=2))
  if not r['records'] or not r['records'][-1]['run_pass']:a.exit(2,'Latest Stage A receipt not PASS; evidence preserved.\n')
 except (OSError,ValueError,p.CollectionError) as e:a.exit(1,str(e)+'\n')
if __name__=='__main__':main()
