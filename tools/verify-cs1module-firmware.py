#!/usr/bin/env python3
"""Verify the module-install/boot-autoarm candidate, without device access."""
import argparse,importlib.util,json,re,subprocess
from pathlib import Path
def module(name,file):
 s=importlib.util.spec_from_file_location(name,Path(__file__).with_name(file));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def verify(elf,binary,uf2,rom,reference,baseline_uf2,boot_reference,dis):
 checker=module('stagefw','verify-cs1-stagea-firmware.py');r=checker.verify(elf,binary,uf2,rom,reference,baseline_uf2)
 e=checker.m.ELF(elf);boot=checker.m.ELF(boot_reference)
 if e.symbol('sb_async_boot_request')!=boot.symbol('sb_async_boot_request'):raise ValueError('tested boot gate differs')
 if not re.search(r'\bbl\S*\s+[0-9a-f]+\s+<sb_async_boot_request>',dis):raise ValueError('autoarm call missing')
 r.update(starts_disarmed=False,initial_flags_disarmed=True,boot_autoarm=True,boot_gate_bytes=len(e.symbol('sb_async_boot_request')),tested_boot_gate_equal=True)
 return r
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('elf','bin','uf2','rom','reference','baseline-uf2','boot-reference','dis','output'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();r=verify(*(getattr(a,k).read_bytes() for k in ('elf','bin','uf2','rom','reference','baseline_uf2','boot_reference')),a.dis.read_text());a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
