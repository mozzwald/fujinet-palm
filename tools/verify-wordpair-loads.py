#!/usr/bin/env python3
"""Require exactly two MOVE.W loads with exactly N unrolled NOPs in each entry."""
import argparse
import hashlib
import json
from pathlib import Path
import re
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('disassembly',type=Path);p.add_argument('prc',type=Path)
a=p.parse_args();text=a.disassembly.read_text();prc=a.prc.read_bytes();entries=[]
if prc[60:68]!=b'applFNVE' or int.from_bytes(prc[32:34],'big')&0x20:
    p.error('requires separate FNVE RAM diagnostic without reset-after-install')
for n in (0,1,2,4,8,16,32,64):
    body=re.search(rf'^[0-9a-f]+ <vc_words_{n}>:\n(.*?)(?=^[0-9a-f]+ <|\Z)',text,re.M|re.S)
    if not body:p.error(f'missing vc_words_{n}')
    code=bytes.fromhex(''.join(re.findall(r'^\s*[0-9a-f]+:\s+((?:[0-9a-f]{4}\s+)+)',body[1],re.M)))
    expected=bytes.fromhex('206f00043010')+bytes.fromhex('4e71')*n+bytes.fromhex('32280002484030014e75')
    if code!=expected:p.error(f'vc_words_{n}: unexpected code between loads or incorrect result assembly')
    if prc.count(code)!=1:p.error(f'vc_words_{n}: packaged code not unique')
    entries.append(dict(symbol=f'vc_words_{n}',nops=n,bytes=len(code),code_sha256=hashlib.sha256(code).hexdigest()))
print(json.dumps(dict(result='PASS',creator='FNVE',prc_sha256=hashlib.sha256(prc).hexdigest(),
    external_loads='MOVE.W (A0),D0; N NOPs; MOVE.W 2(A0),D1',
    entries=entries,reset_after_install=False,physical_timing_verified=False),indent=2))
