#!/usr/bin/env python3
"""Explicit manual arm/read-only monitoring. NEVER flash/reset/stop or reinsert.

Default is read-only. --arm sends one b only after exact candidate identity and
fresh disarmed status. Closing the console leaves the service running.
"""
import argparse,hashlib,json,re,time,zlib
from pathlib import Path
import prism_collect as p
def status(rp):
 rp.send('s');text=rp.read(.3);rows=[line for line in text.splitlines() if line.startswith('active=')]
 if not rows:raise p.CollectionError('Stage A status missing')
 fields=dict(re.findall(r'(\w+)=([0-9a-f]+)',rows[-1]))
 if not {'active','stopped','arm_pending','run','accepted','capture_fault','status','ack','counters_final'}<=fields.keys():raise p.CollectionError('Stage A status incomplete')
 return {k:int(v,16 if k in ('status','ack') else 10) for k,v in fields.items()}
def inspect(rp,rom,arm=False,build_id=None):
 if not build_id or not re.fullmatch(r"[0-9a-f]{64}",build_id):raise ValueError("Exact bundle build identity required")
 rp.send('q');identity=rp.read(.4)
 expected=f'CS1_STAGE_A abi=0100 words={len(rom)//2} crc={zlib.crc32(rom):08x} clock_khz=150000 starts_disarmed=1 marker=none build={build_id}'
 if expected not in identity:raise p.CollectionError('Wrong firmware/ROM/clock; no arm sent')
 before=status(rp)
 if not arm:return before
 if not before['stopped'] or before['active'] or before['arm_pending']:raise p.CollectionError('Require freshly disarmed candidate; running service left alone')
 rp.send('b')
 for _ in range(40):
  now=status(rp)
  if now['active'] and not now['stopped'] and not now['arm_pending'] and now['run']==before['run']+1:return now
 raise p.CollectionError('Arm pending/failed; no automatic stop or retry')
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--bundle',type=Path,required=True);a.add_argument('--log-dir',type=Path,required=True);a.add_argument('--rp');a.add_argument('--arm',action='store_true');a.add_argument('--seconds',type=float,default=60);x=a.parse_args();rp=None
 try:
  b=json.loads(x.bundle.read_text())
  if b['kind']!='visor-cs1-stage-a' or not b['firmware_proof']['starts_disarmed'] or not b['preflash_pass']:raise ValueError('Candidate bundle not qualified for manual Stage A test')
  for name,item in b['artifacts'].items():
   path=Path(item['path'])
   if path.name!=str(path) or hashlib.sha256((x.bundle.parent/path).read_bytes()).hexdigest()!=item['sha256']:raise ValueError(name+' hash mismatch')
  rom=(x.bundle.parent/b['artifacts']['rom']['path']).read_bytes();x.log_dir.mkdir(parents=True,exist_ok=False)
  rp=p.RP(p.rp_port(x.rp),x.log_dir/'console.log');s=inspect(rp,rom,x.arm,b['firmware_proof']['build_id']);p.atomic_json(x.log_dir/'initial.json',s);print(json.dumps(s),flush=True)
  end=time.monotonic()+x.seconds
  while time.monotonic()<end:
   time.sleep(2);s=status(rp);p.atomic_json(x.log_dir/'latest.json',s);print(json.dumps(s),flush=True)
 except (OSError,ValueError,KeyError,p.CollectionError,KeyboardInterrupt) as e:a.exit(1,f'INCOMPLETE: {e}; no automatic stop/flash/reset performed.\n')
 finally:
  if rp:rp.close()
if __name__=='__main__':main()
