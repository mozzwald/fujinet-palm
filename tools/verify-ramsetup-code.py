#!/usr/bin/env python3
"""Check linked Palm text/resources, reachable A5 independence and proof opcodes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib

def verify(prc,text,nm,dis,sections):
    symbols={m[2]:int(m[1],16) for m in re.finditer(r'^([0-9a-f]+)\s+[Tt]\s+(\S+)$',nm,re.M)}
    entries=[struct.unpack_from('>4sHI',prc,78+10*i) for i in range(int.from_bytes(prc[76:78],'big'))]
    resources={(t,n):prc[o:entries[i+1][2] if i+1<len(entries) else len(prc)] for i,(t,n,o) in enumerate(entries)}
    if resources.get((b'code',1))!=text:raise ValueError('code1 is not the exact linked .text')
    for section in ('.data','.bss'):
        m=re.search(r'\d+\s+'+re.escape(section)+r'\s+([0-9a-f]+)',sections)
        if not m or int(m[1],16)!=0:raise ValueError('application globals/BSS present')
    pc=symbols['fnram_pc'];start=symbols['fnram_loop'];end=symbols['fnram_loop_end']
    if text[pc:pc+8]!=bytes.fromhex('41fafffe20084e75'):raise ValueError('PC marker is not self-relative LEA')
    if text[start:end]!=bytes.fromhex('727f303cffff4e7151c8fffc51c9fff4203c52504c504e75'):
        raise ValueError('RAM loop differs: require two DBRA loops, NOP, result, RTS; no traps or bus data accesses')
    # objdump also decodes inline strings as instructions. Follow reachable
    # instructions, stopping at RTS and skipping the inline Palm trap word.
    ins={};previous=None
    for line in dis.splitlines():
        fields=line.split('\t')
        if len(fields)<2 or not re.fullmatch(r'\s*[0-9a-f]+:',fields[0]):continue
        at=int(fields[0].strip()[:-1],16);raw=fields[1].split()
        if not raw or not all(re.fullmatch(r'[0-9a-f]{4}',w) for w in raw):continue
        op='\t'.join(fields[2:]).strip()
        if op:
            ins[at]=(2*len(raw),op);previous=at
        elif previous is not None and previous+ins[previous][0]==at:
            size,op=ins[previous];ins[previous]=(size+2*len(raw),op)
    roots=[v for k,v in symbols.items() if k.startswith('rp_') or k in ('PilotMain','owned','result','cleanup','fnram_pc','fnram_loop')]
    visited=set();pending=list(roots)
    while pending:
        at=pending.pop()
        if at in visited:continue
        if at not in ins:raise ValueError(f'reachable undecoded instruction {at:x}')
        visited.add(at);size,op=ins[at];mn=op.split()[0]
        if re.search(r'%a5\b',op):raise ValueError(f'A5-dependent application instruction {at:x}: {op}')
        if mn in ('rts','rte'):continue
        target=re.search(r'\b([0-9a-f]+)\s+<',op)
        if mn.startswith(('bra','jmp')):
            if target:pending.append(int(target[1],16))
            continue
        if mn.startswith(('bsr','jsr')) and target:pending.append(int(target[1],16))
        if (mn.startswith('b') and not mn.startswith('bsr')) or mn.startswith('db'):
            if target:pending.append(int(target[1],16))
        if mn=='trap' and '#15' in op:pending.append(at+4)
        else:pending.append(at+size)
    return dict(result='PASS',text_bytes=len(text),text_sha256=hashlib.sha256(text).hexdigest(),
                code_crc32=f'{zlib.crc32(text):08x}',pc_marker_offset=pc,loop_start_offset=start,
                loop_end_offset=end,reachable_application_instructions=len(visited),
                globals_bytes=0,bss_bytes=0,application_A5_dependencies=0,
                proof='exact linked text in code1; self-relative marker and exact trap-free loop',hardware_verified=False)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('prc','text','nm','dis','sections','output'):p.add_argument('--'+k,type=Path,required=True)
    a=p.parse_args()
    try:r=verify(a.prc.read_bytes(),a.text.read_bytes(),a.nm.read_text(),a.dis.read_text(),a.sections.read_text())
    except (KeyError,ValueError,struct.error) as e:p.exit(1,f'RAM code verification failed: {e}\n')
    a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
if __name__=='__main__':main()
