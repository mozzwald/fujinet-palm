#!/usr/bin/env python3
"""Check actual m68k stack arguments for the 128-byte diagnostic result.

Deliberately strict for the reviewed -O2 m68k build: a compiler/register allocation
change requires reviewing the call sequence, rather than accepting a nearby
unrelated 128 literal. No hardware, ROM modification or database installation.
"""
import argparse,json,re
from pathlib import Path

def body(dis,name):
 m=re.search(r'^\s*[0-9a-f]+ <'+re.escape(name)+r'>:\n(.*?)(?=^\s*[0-9a-f]+ <[^>]+>:\n|\Z)',dis,re.M|re.S)
 if not m:raise ValueError('missing function '+name)
 raw=[]
 for line in m[1].splitlines():
  fields=line.split('\t')
  if len(fields)<2:continue
  words=fields[1].split()
  if words and all(re.fullmatch('[0-9a-f]{4}',w) for w in words):raw.extend(words)
 return bytes.fromhex(''.join(raw))

def verify(dis):
 checks={
  # value0 byte, size128 long, buffer a2, MemSet trap
  'clear128':('snapshot','4227487800802f0a4e4fa027'),
  # size128 long, index address, database d5, DmNewRecord trap
  'allocate128':('save','48780080486efff42f054e4fa055'),
  # size128 long, source a2, offset0, destination d0, DmWrite trap
  'write128':('save','487800802f0a42a72f004e4fa076'),
 }
 for key,(name,raw) in checks.items():
  if body(dis,name).count(bytes.fromhex(raw))!=1:raise ValueError('result buffer call sequence mismatch: '+key)
 return dict(result='PASS',bytes=128,word_count=32,clear128=True,allocate128=True,write128=True,physical_cs1_qualified=False)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('dis',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=verify(a.dis.read_text());a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
