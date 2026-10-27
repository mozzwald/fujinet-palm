#!/usr/bin/env python3
"""Verify the separate RAM timing PRC and exact 68000 probe instructions."""
import argparse,hashlib,json,re,struct,zlib
from pathlib import Path
def verify(prc,text,nm):
 if prc[:32].split(b'\0')[0]!=b'Prism Timing P3' or prc[60:68]!=b'applFNTM' or int.from_bytes(prc[32:34],'big')!=1:raise ValueError('wrong RAM app identity/attributes')
 e=[struct.unpack_from('>4sHI',prc,78+10*i) for i in range(int.from_bytes(prc[76:78],'big'))]
 res={(t,n):prc[o:e[k+1][2] if k+1<len(e) else len(prc)] for k,(t,n,o) in enumerate(e)}
 if res.get((b'code',1))!=text:raise ValueError('linked text differs from code1')
 s={m[2]:int(m[1],16) for m in re.finditer(r'^([0-9a-f]+)\s+[Tt]\s+(\S+)$',nm,re.M)}
 if text[s['vt_pc']:s['vt_pc']+8]!=bytes.fromhex('41fafffe20084e75'):raise ValueError('PC marker is not PC-relative')
 op=bytes.fromhex('206f0004226f0008301032803028000233400002226f000c201022804e75')
 if text[s['vt_loads']:s['vt_loads']+len(op)]!=op:raise ValueError('require two MOVE.W reads then exactly one aligned MOVE.L read')
 return dict(kind='visor-phase3-ram-timing',name='Prism Timing P3',creator='FNTM',text_bytes=len(text),code_crc32=f'{zlib.crc32(text):08x}',pc_marker_offset=s['vt_pc'],loads_offset=s['vt_loads'],prc_sha256=hashlib.sha256(prc).hexdigest(),physical_timing_verified=False)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('prc','text','nm','output'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();r=verify(a.prc.read_bytes(),a.text.read_bytes(),a.nm.read_text());a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
