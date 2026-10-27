#!/usr/bin/env python3
"""Independent ELF/BIN/UF2/ROM linkage and tested ARM body identity; no device I/O."""
import argparse,hashlib,importlib.util,json,struct
from pathlib import Path
s=importlib.util.spec_from_file_location('ramfw',Path(__file__).with_name('verify-ramsetup-firmware.py'));m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def verify(elf,binary,uf2,rom,reference,baseline_uf2):
 e=m.ELF(elf);base,flash=e.flash()
 if base!=0x10000000 or flash!=binary:raise ValueError('ELF/BIN disagreement')
 if e.symbol('sb_rom_image')!=rom:raise ValueError('embedded ROM mismatch')
 if e.symbol('sb_async_reply')!=m.ELF(reference).symbol('sb_async_reply'):raise ValueError('tested ARM instructions differ')
 if any(name in e.symbols for name in ('sb_persistent_reply','sb_cs1_write_step','sb_boot_autoarm_entry')):raise ValueError('wrong service linked')
 if e.symbol('stopped')!=b'\1' or e.symbol('arm')!=b'\0' or e.symbol('active')!=b'\0':raise ValueError('not disarmed')
 for name in ('words','table'):
  address,n,_,_=e.symbols[name]
  if n!=65536 or address%2 or not 0x20000000<=address<address+n<=0x20082000:raise ValueError('SRAM table placement')
 if len(uf2)%512 or uf2[:512]!=baseline_uf2[:512]:raise ValueError('UF2 absolute block differs')
 first=struct.unpack_from('<8I',uf2)
 if first[2:4]!=(0xa000,0x10ffff00) or first[7]!=0xe48bff57:raise ValueError('absolute block type')
 padded=binary+bytes((-len(binary))%256);count=len(padded)//256
 if len(uf2)!=(count+1)*512:raise ValueError('UF2 page count')
 seen=set()
 for i in range(count):
  b=uf2[(i+1)*512:(i+2)*512];a,z,flags,addr,n,number,total,family=struct.unpack_from('<8I',b)
  if (a,z,flags,n,total,family)!=(0x0a324655,0x9e5d5157,0x2000,256,count,0xe48bff59) or struct.unpack_from('<I',b,508)[0]!=0x0ab16f30:raise ValueError('UF2 headers')
  if number in seen or number>=count or addr!=base+number*256:raise ValueError('UF2 address ownership')
  seen.add(number)
  if b[32:288]!=padded[number*256:(number+1)*256]:raise ValueError('UF2 payload differs')
 sha=lambda b:hashlib.sha256(b).hexdigest()
 return dict(result='PASS',starts_disarmed=True,elf_bin_uf2_equal=True,tested_arm_body_equal=True,rom_equal=True,rom_sha256=sha(rom),elf_sha256=sha(elf),bin_sha256=sha(binary),uf2_sha256=sha(uf2),arm_body_sha256=sha(e.symbol('sb_async_reply')),arm_body_bytes=len(e.symbol('sb_async_reply')),build_id=e.symbol('sb_async_build_id').rstrip(b'\0').decode('ascii'),words_address=e.symbols['words'][0],table_address=e.symbols['table'][0],clock_khz_source=150000,physical_cs1_qualified=False)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('elf','bin','uf2','rom','reference','baseline-uf2','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();r=verify(*(getattr(a,k).read_bytes() for k in ('elf','bin','uf2','rom','reference','baseline_uf2')));a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
