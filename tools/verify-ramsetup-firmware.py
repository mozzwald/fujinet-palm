#!/usr/bin/env python3
"""Independently link ELF -> BIN -> UF2 and compare frozen critical functions.

Accept only the exact baseline RP2350-E10 ignored absolute block, followed by
RP2350 ARM secure image blocks. This tool never communicates with hardware.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

def sha(b):return hashlib.sha256(b).hexdigest()
class ELF:
    def __init__(self,b):
        if b[:6]!=b'\x7fELF\x01\x01' or struct.unpack_from('<H',b,18)[0]!=40:raise ValueError('require ELF32 little-endian ARM')
        self.b=b
        off=struct.unpack_from('<I',b,32)[0];size,count=struct.unpack_from('<HH',b,46)
        if size!=40:raise ValueError('ELF section header size')
        self.sections=[struct.unpack_from('<10I',b,off+i*size) for i in range(count)]
        self.symbols={}
        for section in self.sections:
            if section[1]!=2:continue
            strings=self.sections[section[6]];names=b[strings[4]:strings[4]+strings[5]]
            for at in range(section[4],section[4]+section[5],16):
                name,value,n,info,other,index=struct.unpack_from('<IIIBBH',b,at)
                if name and index<len(self.sections):
                    label=names[name:names.find(b'\0',name)].decode('ascii')
                    self.symbols[label]=(value,n,info&15,index)
    def symbol(self,name):
        value,n,kind,index=self.symbols[name];section=self.sections[index];at=((value&~1) if kind==2 else value)-section[3]
        if section[1]==8:return bytes(n)
        if not 0<=at<=section[5]-n:raise ValueError('symbol bounds')
        return self.b[section[4]+at:section[4]+at+n]
    def memory(self,address,n):
        for section in self.sections:
            if section[3]<=address and address+n<=section[3]+section[5] and section[1]!=8:
                at=section[4]+address-section[3];return self.b[at:at+n]
        raise ValueError('address is not in stored ELF section')
    def flash(self):
        off=struct.unpack_from('<I',self.b,28)[0];size,count=struct.unpack_from('<HH',self.b,42)
        segments=[]
        for i in range(count):
            t,o,v,p,n,m,flags,align=struct.unpack_from('<8I',self.b,off+i*size)
            if t==1 and n and 0x10000000<=p<0x11000000:segments.append((p,self.b[o:o+n]))
        base=min(p for p,_ in segments);end=max(p+len(d) for p,d in segments)
        output=bytearray(end-base)
        for p,data in segments:output[p-base:p-base+len(data)]=data
        return base,bytes(output)

def verify(elf,binary,uf2,baseline_elf,baseline_uf2,rom):
    e,old=ELF(elf),ELF(baseline_elf);base,flash=e.flash()
    if base!=0x10000000 or flash!=binary:raise ValueError('BIN differs from ELF flash load segments')
    if e.symbol('sb_rom_image')!=rom:raise ValueError('embedded ROM differs from independently verified input')
    critical={}
    name='sb_persistent_reply';b=e.symbol(name);prior=old.symbol(name)
    if b!=prior:raise ValueError('frozen ARM response kernel changed')
    critical[name]=dict(bytes=len(b),sha256=sha(b),baseline_address=old.symbols[name][0],candidate_address=e.symbols[name][0],byte_identical=True)
    name='sb_pio_rearm_program_init';b=e.symbol(name);prior=old.symbol(name)
    # Exactly one literal points to the 32-word initialization template located
    # after the embedded ROM. Its data address moves with the larger ROM.
    # Every other initializer byte and all 64 target bytes must remain equal.
    at=0xac
    if len(b)!=928 or len(prior)!=928 or b[:at]+b[at+4:]!=prior[:at]+prior[at+4:]:
        raise ValueError('PIO initialization differs beyond the one known data-address relocation')
    address=struct.unpack_from('<I',b,at)[0];old_address=struct.unpack_from('<I',prior,at)[0]
    program=e.memory(address,64);old_program=old.memory(old_address,64)
    if program!=old_program:raise ValueError('PIO instruction template changed')
    critical[name]=dict(bytes=len(b),sha256=sha(b),baseline_sha256=sha(prior),byte_identical=b==prior,
        baseline_address=old.symbols[name][0],candidate_address=e.symbols[name][0],
        instructions_equal=True,relocation_offset=at,baseline_template_address=old_address,
        candidate_template_address=address,template_bytes=64,template_sha256=sha(program))
    if e.symbols['words'][1]!=65536 or old.symbols['words'][1]!=65536 or e.symbols['words'][0]%16!=old.symbols['words'][0]%16:
        raise ValueError('SRAM lookup capacity or bank alignment changed')
    if any(name.startswith('sb_cs1') for name in e.symbols):raise ValueError('experimental CS1 code linked')
    if e.symbol('stopped')!=b'\1' or e.symbol('arm')!=b'\0' or e.symbol('active')!=b'\0':raise ValueError('candidate does not start disarmed')
    if len(uf2)%512 or len(uf2)<1024:raise ValueError('UF2 size')
    if uf2[:512]!=baseline_uf2[:512]:raise ValueError('RP2350 absolute/ignore block differs from proven baseline')
    first=struct.unpack_from('<8I',uf2)
    if first[2]!=0xa000 or first[3]!=0x10ffff00 or first[7]!=0xe48bff57:raise ValueError('baseline absolute block type')
    image=uf2[512:];count=len(image)//512;expected=binary+bytes((-len(binary))%256);seen=set()
    if count*256!=len(expected):raise ValueError('UF2/BIN page count')
    for i in range(count):
        block=image[i*512:(i+1)*512];m0,m1,flags,addr,n,number,total,family=struct.unpack_from('<8I',block)
        if (m0,m1,flags,n,total,family)!=(0x0a324655,0x9e5d5157,0x2000,256,count,0xe48bff59):raise ValueError('UF2 header/family')
        if struct.unpack_from('<I',block,508)[0]!=0x0ab16f30:raise ValueError('UF2 end magic')
        if number in seen or number>=count or addr!=base+number*256:raise ValueError('UF2 block ownership/address')
        seen.add(number)
        if block[32:288]!=expected[number*256:(number+1)*256]:raise ValueError('UF2 payload differs from ELF/BIN')
    return dict(result='PASS',critical_functions_equal=True,critical_functions=critical,
                starts_disarmed=True,experimental_cs1_linked=False,baseline_words_address=old.symbols['words'][0],candidate_words_address=e.symbols['words'][0],lookup_alignment_mod16=e.symbols['words'][0]%16,elf_bin_uf2_equal=True,embedded_rom_equal=True,
                absolute_ignore_block_equal=True,absolute_block_sha256=sha(uf2[:512]),uf2_family='RP2350_ARM_S',
                elf_sha256=sha(elf),bin_sha256=sha(binary),uf2_sha256=sha(uf2),flash_base=base,flash_bytes=len(binary),
                hardware_verified=False)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('elf','bin','uf2','baseline-elf','baseline-uf2','rom','output'):p.add_argument('--'+k,type=Path,required=True)
    a=p.parse_args()
    try:r=verify(a.elf.read_bytes(),a.bin.read_bytes(),a.uf2.read_bytes(),a.baseline_elf.read_bytes(),a.baseline_uf2.read_bytes(),a.rom.read_bytes())
    except (ValueError,KeyError,struct.error) as e:p.exit(1,f'Firmware verification failed: {e}\n')
    a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
if __name__=='__main__':main()
