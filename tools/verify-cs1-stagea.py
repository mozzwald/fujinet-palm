#!/usr/bin/env python3
"""Check RAM Stage A identity, code resource, PC marker and actual MOVE.W helpers."""
import argparse,hashlib,json,re,struct,zlib
from pathlib import Path
def verify(prc,text,nm,abi,name=b'CS1 Stage A',dbtag=b'applFNMA'):
 if prc[:32].split(b'\0')[0]!=name or prc[60:68]!=dbtag or int.from_bytes(prc[32:34],'big')!=1:raise ValueError('RAM Stage A metadata')
 entries=[struct.unpack_from('>4sHI',prc,78+10*i) for i in range(int.from_bytes(prc[76:78],'big'))]
 resources={(t,n):prc[o:entries[k+1][2] if k+1<len(entries) else len(prc)] for k,(t,n,o) in enumerate(entries)}
 if resources.get((b'code',1))!=text:raise ValueError('linked code1 mismatch')
 symbols={m[2]:int(m[1],16) for m in re.finditer(r'^([0-9a-f]+)\s+[Tt]\s+(\S+)$',nm,re.M)}
 ops={'va_pc':'41fafffe20084e75','va_read':'4e560000206e00084280302e000c303008004e5e4e75','va_write':'4e560000206e0008322e000e4280302e000c318108004e5e4e75'}
 for name,op in ops.items():
  b=bytes.fromhex(op)
  if text[symbols[name]:symbols[name]+len(b)]!=b:raise ValueError('word-access/PC opcode mismatch: '+name)
 return dict(kind='visor-cs1-stage-a',text_bytes=len(text),code_crc32=f'{zlib.crc32(text):08x}',pc_marker_offset=symbols['va_pc'],prc_sha256=hashlib.sha256(prc).hexdigest(),abi_sha256=hashlib.sha256(abi).hexdigest(),peripheral_access='verified MOVE.W only',physical_cs1_qualified=False)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('prc','text','nm','abi','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();r=verify(a.prc.read_bytes(),a.text.read_bytes(),a.nm.read_text(),a.abi.read_bytes());a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
