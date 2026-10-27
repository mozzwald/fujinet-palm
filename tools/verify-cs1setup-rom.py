#!/usr/bin/env python3
"""Independent parser for the separate one-Setup-database Phase2 image.

Does not import the PRC metadata writer or MakeROM. No baseline verifier changes.
"""
import argparse
import binascii
import hashlib
import json
from pathlib import Path
import struct
import zlib

def verify(data,prc):
    def need(ok,msg):
        if not ok:raise ValueError(msg)
    def n(off,size=4):
        need(0<=off<=len(data)-size,'truncated ROM field')
        return int.from_bytes(data[off:off+size],'big')
    def ptr(value):
        p=value-0x08000000;need(0<=p<len(data),'invalid card-relative chunk pointer');return p
    used=[]
    def chunk(p):
        need(p>=8 and p%2==0,'chunk alignment/header')
        h=n(p-8);size=(h&0xffffff)-8-((h>>24)&15)
        need(not h&0x80000000 and size>=0 and p+size<=len(data),'bad resource chunk')
        need(all(p+size<=a or b<=p-8 for a,b in used),'overlapping resource chunks')
        used.append((p-8,p+size));return data[p:p+size]
    need(512<=len(data)<=65536 and len(data)%2==0,'ROM size')
    need(n(8)==0xfeedbeef and n(12,2)==4 and n(256)==0xfeedface,'Palm card/store signatures')
    need(n(108)==len(data),'CRC coverage')
    crc=binascii.crc_hqx(data[:112],0);crc=binascii.crc_hqx(data[114:],crc)
    need(crc==n(112,2),'ROM CRC16')
    tok=ptr(n(100));need(data[tok:tok+14]==b'HsAT\x00\x03330'+b'\xff'*5,'only HsAT330; no Welcome tokens');need(tok+14==len(data),'unexpected token tail')
    directory=ptr(n(316));need(n(directory)==0 and n(directory+4,2)==1,'exactly one database; no continuation')
    db=ptr(n(directory+6));need(db+78<=len(data),'database header bounds')
    need(len(prc)>=78 and prc[:32].split(b'\0')[0] in (b'FN CS1 Setup A',b'FN CS1 Setup B'),'PRC identity')
    revision='B' if prc[:32].split(b'\0')[0]==b'FN CS1 Setup B' else 'A'
    need(prc[60:68]==b'HsSUHsCd' and data[db+60:db+68]==b'HsSUHsCd','SDK Setup type/creator (case sensitive)')
    need(prc[:32]==data[db:db+32] and n(db+34,2)==1 and int.from_bytes(prc[34:36],'big')==1,'database name/version')
    need(int.from_bytes(prc[32:34],'big')==1 and n(db+32,2)==3,'only resource/read-only attrs; no reset/welcome/application')
    need(n(db+72)==0 and prc[72:76]==bytes(4),'unexpected database continuation')
    count=int.from_bytes(prc[76:78],'big');need(count==n(db+76,2) and count>2,'resource count')
    need(78+10*count<=len(prc) and db+78+10*count<=len(data),'resource directory bounds')
    entries=[struct.unpack_from('>4sHI',prc,78+10*i) for i in range(count)]
    need(len({(t,i) for t,i,_ in entries})==count,'duplicate resource IDs')
    need(all(78+10*count<=e[2]<len(prc) for e in entries),'PRC resource offset')
    need(all(a[2]<b[2] for a,b in zip(entries,entries[1:])),'PRC resource ordering')
    resources=[]
    for i,(kind,ident,start) in enumerate(entries):
        end=entries[i+1][2] if i+1<count else len(prc);off=db+78+10*i
        need(data[off:off+6]==prc[78+10*i:84+10*i],'resource type/ID mismatch')
        p=ptr(n(off+6));payload=chunk(p);need(payload==prc[start:end],'resource payload mismatch')
        if (kind,ident)==(b'rP2I',1000):need(payload==(b'FN CS1 B 000002\0' if revision=='B' else b'FN CS1 A 000001\0')+bytes(8),'identity/alignment padding differs')
        resources.append(dict(type=kind.decode('latin1'),id=ident,offset=p,size=len(payload),sha256=hashlib.sha256(payload).hexdigest(),crc32=f'{zlib.crc32(payload):08x}'))
    need(any(r['type']=='code' and r['id']==1 for r in resources),'code1 missing')
    need(any(r['type']=='tFRM' and r['id']==1000 for r in resources),'runtime form missing')
    need(any(r['type']=='rP2I' and r['id']==1000 for r in resources),'build identity missing')
    return dict(kind='cs1-module-setup',hardware_verified=False,access_time_ns=330,size=len(data),
                sha256=hashlib.sha256(data).hexdigest(),crc16=f'{crc:04x}',crc32=f'{zlib.crc32(data):08x}',
                prc_sha256=hashlib.sha256(prc).hexdigest(),database_offset=db,type='HsSU',creator='HsCd',
                resources=resources,expected_runtime_name='CS1 Module '+revision,expected_runtime_creator='FNMC' if revision=='B' else 'FNMB')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom',type=Path);p.add_argument('--prc',type=Path,required=True);p.add_argument('--manifest',type=Path)
    a=p.parse_args()
    try:r=verify(a.rom.read_bytes(),a.prc.read_bytes())
    except (ValueError,OSError,struct.error) as e:p.exit(1,f'RAM Setup ROM invalid: {e}\n')
    s=json.dumps(r,indent=2)+'\n'
    if a.manifest:a.manifest.write_text(s)
    print(s,end='')
if __name__=='__main__':main()
