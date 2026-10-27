#!/usr/bin/env python3
"""Read-only watch for one-shot autoarm module firmware. Never sends b/x/reset."""
import argparse,hashlib,json,time,zlib
from pathlib import Path
import prism_collect as p
from cs1_stagea_service import status
def inspect(rp,rom,build_id):
 rp.send('q');text=rp.read(.4)
 expected=f'CS1_STAGE_A abi=0100 words={len(rom)//2} crc={zlib.crc32(rom):08x} clock_khz=150000 starts_disarmed=0 marker=none boot_autoarm=1'
 if text.strip()!=expected:raise p.CollectionError('Wrong module candidate; state unchanged')
 rp.send('i');identity=rp.read(.4)
 if identity.strip()!='BUILD '+build_id:raise p.CollectionError('Full build identity mismatch; state unchanged')
 return status(rp)
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--bundle',type=Path,required=True);a.add_argument('--log-dir',type=Path,required=True);a.add_argument('--rp');a.add_argument('--seconds',type=float,default=1800);x=a.parse_args();rp=None
 try:
  b=json.loads(x.bundle.read_text())
  if b['kind']!='visor-cs1-module-autoarm' or not b['firmware_proof']['tested_boot_gate_equal']:raise ValueError('Wrong bundle or missing boot proof')
  for name,item in b['artifacts'].items():
   f=Path(item['path'])
   if f.name!=str(f) or hashlib.sha256((x.bundle.parent/f).read_bytes()).hexdigest()!=item['sha256']:raise ValueError(name+' hash mismatch')
  rom=(x.bundle.parent/b['artifacts']['rom']['path']).read_bytes();x.log_dir.mkdir(parents=True,exist_ok=False)
  rp=p.RP(p.rp_port(x.rp),x.log_dir/'console.log');s=inspect(rp,rom,b['firmware_proof']['build_id']);p.atomic_json(x.log_dir/'initial.json',s)
  end=time.monotonic()+x.seconds
  while True:
   p.atomic_json(x.log_dir/'latest.json',s);print(json.dumps(s),flush=True)
   if time.monotonic()>=end:break
   time.sleep(1);s=status(rp)
 except (OSError,ValueError,KeyError,p.CollectionError,KeyboardInterrupt) as e:a.exit(1,f'INCOMPLETE: {e}; no arm/stop/flash/reset was sent.\n')
 finally:
  if rp:rp.close()
if __name__=='__main__':main()
