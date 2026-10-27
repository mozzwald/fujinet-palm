#!/usr/bin/env python3
"""Read-only status for boot-autoarm firmware. Never sends arm/stop/reset."""
import argparse,hashlib,json,zlib
from pathlib import Path
import prism_collect as p

def inspect(rp,rom):
 rp.send('q');identity=rp.read(.4)
 expected=f'CONTINUOUS version=3 response=cpu-rom marker=preload continuous=1 words={len(rom)//2} crc={zlib.crc32(rom):08x} clock_khz=150000 boot_autoarm=1'
 if expected not in identity:raise p.CollectionError('autoarm image/ROM identity mismatch; state unchanged')
 state=rp.status()
 if not state['rom_ready'] or not state['ready']:raise p.CollectionError('ROM verification or owner fault; state unchanged')
 return dict(identity=identity,status=state,service='ACTIVE' if state['active'] else 'automatic arm pending / waiting for idle controls or reset' if state['arm_pending'] or not state['stopped'] else 'STOPPED; no automatic retry after deliberate stop/fault')

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--manifest',type=Path,required=True);a.add_argument('--log-dir',type=Path,required=True);a.add_argument('--rp');args=a.parse_args();rp=None
 try:
  m=json.loads(args.manifest.read_text());item=m['artifacts']['rom'];rom=(args.manifest.parent/item['path']).read_bytes()
  if m.get('kind')!='ramsetup-boot-autoarm' or hashlib.sha256(rom).hexdigest()!=item['sha256']:raise ValueError('bundle/ROM hash mismatch')
  args.log_dir.mkdir(parents=True,exist_ok=False);rp=p.ContinuousRP(p.rp_port(args.rp),args.log_dir/'console.log')
  r=inspect(rp,rom);p.atomic_json(args.log_dir/'status.json',r);print(json.dumps(r,indent=2))
 except (OSError,ValueError,KeyError,p.CollectionError) as e:a.exit(1,f'INCOMPLETE: {e}; no arm/stop/reset sent.\n')
 finally:
  if rp:rp.close()
if __name__=='__main__':main()
