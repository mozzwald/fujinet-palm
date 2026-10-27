#!/usr/bin/env python3
"""Convert only the new build-prc output into SDK HsCd/HsSU Setup metadata."""
import argparse
from pathlib import Path
import struct

def prepare(data):
    b=bytearray(data)
    if len(b)<78 or b[:32].split(b'\0')[0]!=b'FN RAM Setup P2' or b[60:68]!=b'applHsCd':
        raise ValueError('expected new FN RAM Setup P2 appl/HsCd build-prc input')
    if not struct.unpack_from('>H',b,32)[0]&1:raise ValueError('resource database required')
    struct.pack_into('>HH',b,32,1,1)
    b[36:52]=bytes(16)
    b[60:68]=b'HsSUHsCd'
    return bytes(b)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('input',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args()
    if a.input.resolve()==a.output.resolve():p.error('input/output must be separate')
    a.output.write_bytes(prepare(a.input.read_bytes()))
if __name__=='__main__':main()
