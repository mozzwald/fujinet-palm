#!/usr/bin/env python3
"""Auto-arm-specific provenance gate; reuses strict ELF/BIN/UF2/engine checks."""
import argparse,hashlib,importlib.util,json,struct
from pathlib import Path
spec=importlib.util.spec_from_file_location('ramfw',Path(__file__).with_name('verify-ramsetup-firmware.py'));fw=importlib.util.module_from_spec(spec);spec.loader.exec_module(fw)
def verify(elf,binary,uf2,oldelf,olduf2,rom,startup):
 r=fw.verify(elf,binary,uf2,oldelf,olduf2,rom);e=fw.ELF(elf)
 for function in r['critical_functions'].values():
  if function['candidate_address']!=function['baseline_address']:raise ValueError('critical engine placement changed')
 expected=struct.pack('<4I',e.symbols['rom_ready'][0],e.symbols['arm'][0],e.symbols['service'][0]|1,0)
 if e.symbol('sb_boot_policy')!=expected:raise ValueError('boot policy pointers differ')
 # Exact one-shot entry opcodes. Its final pool word is a checked policy pointer.
 if e.symbol('sb_boot_autoarm_entry')!=bytes.fromhex('05480168097821b141680122bff35f8f0a7080680047'):
  raise ValueError('one-shot entry instructions changed')
 if e.memory((e.symbols['sb_boot_autoarm_entry'][0]&~1)+24,4)!=struct.pack('<I',e.symbols['sb_boot_policy'][0]):raise ValueError('entry policy literal differs')
 if struct.pack('<I',e.symbols['sb_boot_autoarm_entry'][0]|1) not in e.symbol('main'):raise ValueError('main does not reference autoarm entry')
 if b'boot_autoarm=1' not in elf:raise ValueError('autoarm identity missing')
 if startup.get('result')!='PASS' or startup.get('elf_sha256')!=fw.sha(elf) or startup.get('rom_sha256')!=fw.sha(rom) or len(startup.get('cases',{}))!=8:raise ValueError('compiled startup proof mismatch')
 # Old gate checks cold initial data bytes; this candidate deliberately requests
 # arm automatically after ROM verification. Do not label it boot-disarmed.
 r['cold_initial_state_disarmed']=r.pop('starts_disarmed');r['starts_disarmed']=False
 r.update(kind='ramsetup-boot-autoarm',autoarm_once_per_boot=True,host_console_required=False,critical_addresses_equal=True,compiled_startup_cases=8,hotplug_qualified=False)
 return r

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('elf','bin','uf2','baseline-elf','baseline-uf2','rom','startup','output'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args()
 try:r=verify(a.elf.read_bytes(),a.bin.read_bytes(),a.uf2.read_bytes(),a.baseline_elf.read_bytes(),a.baseline_uf2.read_bytes(),a.rom.read_bytes(),json.loads(a.startup.read_text()))
 except (OSError,ValueError,KeyError,struct.error) as e:p.exit(1,f'Autoarm verification failed: {e}\n')
 a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
if __name__=='__main__':main()
