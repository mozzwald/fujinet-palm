#!/usr/bin/env python3
"""Check the linked and packaged RAM diagnostic's exact single-load function."""
import argparse
import hashlib
import json
import re
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('disassembly',type=Path);p.add_argument('prc',type=Path)
a=p.parse_args();text=a.disassembly.read_text();prc=a.prc.read_bytes()
body=re.search(r'^[0-9a-f]+ <vc_pair_load>:\n(.*?)(?=^[0-9a-f]+ <|\Z)',text,re.M|re.S)
if not body:p.error('linked vc_pair_load missing')
code=''.join(re.findall(r'^\s*[0-9a-f]+:\s+((?:[0-9a-f]{4}\s+)+)',body[1],re.M))
code=bytes.fromhex(code)
expected=bytes.fromhex('206f000420104e75') # movea.l 4(sp),a0; move.l (a0),d0; rts
if code!=expected:p.error('load function is not the exact one-MOVE.L implementation')
caller=re.search(r'^[0-9a-f]+ <step>:\n(.*?)(?=^[0-9a-f]+ <|\Z)',text,re.M|re.S)
if not caller or len(re.findall(r'\bbsr\w* .*<vc_pair_load>',caller[1]))!=1:
    p.error('expected exactly one call from paired step')
if prc[60:68]!=b'applFNVD' or int.from_bytes(prc[32:34],'big')&0x20:
    p.error('requires RAM FNVD PRC without reset-after-install flag')
if prc.count(expected)!=1:p.error('packaged PRC lacks unique verified load function')
print(json.dumps(dict(result='PASS',opcode_bytes=code.hex(),ROM_load='MOVE.L (A0),D0',
    instructions=3,ROM_loads_per_call=1,prc_sha256=hashlib.sha256(prc).hexdigest(),
    reset_after_install=False,physical_timing_verified=False),indent=2))
