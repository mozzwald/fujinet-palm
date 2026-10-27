#!/usr/bin/env python3
"""One serial HotSync retrieves module installation and RAM CS1 results; no upload."""
import argparse,json,struct,zlib
from pathlib import Path
import prism_collect as p
import cs1_stagea_collect as stage
import ramsetup_results as install
def decode(installation,diagnostic,static):
 rows=[install.decode_record(b,static['setup_proof']) for b in p.pdb_records(installation,static['install_log_name'],b'DATA',creator=static['runtime_creator'].encode())]
 data=stage.decode_pdb(diagnostic,static)
 installed={(r['module'],r['generation'],r['runtime_db']) for r in rows if r['installation_evidence']}
 data['install_records']=rows
 data['matching_install_and_runtime']=bool(data['records'] and data['records'][-1]['run_pass'] and (data['records'][-1]['module'],data['records'][-1]['generation'],data['records'][-1]['current_db']) in installed)
 return data
def verify_clone(app,source,static):
 if app[:32].split(b'\0')[0]!=static['runtime_name'].encode() or app[60:68]!=b'appl'+static['runtime_creator'].encode():raise ValueError('RAM clone identity')
 attrs=int.from_bytes(app[32:34],'big')
 if attrs&~0x8000!=9 or int.from_bytes(app[34:36],'big')!=1:raise ValueError('RAM clone attributes/version')
 def resources(b):
  if len(b)<78:raise ValueError('truncated PRC')
  count=int.from_bytes(b[76:78],'big');end=78+count*10
  if not count or end>len(b):raise ValueError('PRC directory bounds')
  es=[struct.unpack_from('>4sHI',b,78+10*i) for i in range(count)]
  if len({(t,n) for t,n,o in es})!=count or any(o<end or o>=len(b) for t,n,o in es) or any(a[2]>=z[2] for a,z in zip(es,es[1:])):raise ValueError('PRC resource layout')
  return {(t,n):b[o:es[i+1][2] if i+1<count else len(b)] for i,(t,n,o) in enumerate(es)}
 actual,expected=resources(app),resources(source)
 if actual!=expected:raise ValueError('RAM clone resource mismatch')
 code=actual[(b'code',1)]
 if len(code)!=static['text_bytes'] or zlib.crc32(code)!=int(static['code_crc32'],16):raise ValueError('RAM clone code mismatch')
 return dict(metadata_valid=True,attrs_hex=f'{attrs:04x}',all_resources_equal=True,code_crc32=f'{zlib.crc32(code):08x}')

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--static',type=Path,required=True);a.add_argument('--output',type=Path,required=True);a.add_argument('--timeout',type=float,default=900);x=a.parse_args();x.output.mkdir(parents=True,exist_ok=False)
 try:
  s=json.loads(x.static.read_text());names=[s['install_log_name'],s['log_name']]
  p.transfer(x.output,x.output,'--fetch',names,x.timeout,transport='palm-sync')
  r=decode((x.output/(names[0]+'.pdb')).read_bytes(),(x.output/(names[1]+'.pdb')).read_bytes(),s)
  p.atomic_json(x.output/'report.json',r);print(json.dumps(r,indent=2))
  if not r['matching_install_and_runtime']:a.exit(2,'Module installation/RAM test proof incomplete; all evidence preserved.\n')
 except (OSError,ValueError,KeyError,p.CollectionError) as e:a.exit(1,str(e)+'\n')
if __name__=='__main__':main()
