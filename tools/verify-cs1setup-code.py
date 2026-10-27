#!/usr/bin/env python3
"""Verify module Setup and its stack-only diagnostic before ROM installation."""
import argparse,importlib.util,json,hashlib
from pathlib import Path
def module(name,file):
 s=importlib.util.spec_from_file_location(name,Path(__file__).with_name(file));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def verify(prc,text,nm,dis,sections,abi):
 revision='B' if prc[:32].split(b'\0')[0]==b'FN CS1 Setup B' else 'A'
 runtime=module('stagecheck','verify-cs1-stagea.py').verify(prc,text,nm,abi,name=('FN CS1 Setup '+revision).encode(),dbtag=b'HsSUHsCd')
 setup=module('setupcheck','verify-ramsetup-code.py').verify(prc,text,nm,dis,sections)
 runtime.update(kind='visor-cs1-module-setup',runtime_name='CS1 Module '+revision,runtime_creator='FNMC' if revision=='B' else 'FNMB',log_name='FN CS1 Module '+revision+' Log',install_log_name='FN CS1 B Install Log' if revision=='B' else 'FN CS1 Install Log',setup_proof=setup)
 return runtime
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('prc','text','nm','dis','sections','abi','output'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();r=verify(a.prc.read_bytes(),a.text.read_bytes(),a.nm.read_text(),a.dis.read_text(),a.sections.read_text(),a.abi.read_bytes());a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
