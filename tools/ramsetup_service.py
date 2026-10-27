#!/usr/bin/env python3
"""Manual-only arm for a statically verified Phase2 ROM; NEVER flashes/resets.

Run only after manual BOOTSEL loading with the Prism physically held in reset.
A running/busy responder is refused, never stopped. After ACTIVE, this tool
closes its console and leaves the autonomous ROM service running quietly.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time
import prism_collect as p

def load_bundle(path,fallback=False):
    m=json.loads(path.read_text())
    if m.get('kind')!='ramsetup-p2' or not m['responder']['critical_functions_equal']:
        raise ValueError('candidate lacks responder equivalence evidence')
    files={}
    for key in ('rom','uf2','static'):
        item=(m['fallback_artifacts'] if fallback and key in ('rom','uf2') else m['artifacts'])[key];name=Path(item['path'])
        if name.name!=str(name):raise ValueError('bundle artifacts must be adjacent basenames')
        data=(path.parent/name).read_bytes()
        if hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError(f'{key} hash mismatch')
        files[key]=data
    if len(files['rom'])%2 or not 512<=len(files['rom'])<=65536:raise ValueError('ROM size')
    return m,files

def prime(rp,rom,logdir,timeout=60,announce=print):
    import zlib
    before=rp.status();p.atomic_json(logdir/'before.json',before)
    if not before['rom_ready'] or not before['ready'] or not before['stopped'] or before['arm_pending'] or before['active']:
        raise p.CollectionError('Require freshly flashed, ready, DISARMED candidate; running service left untouched')
    rp.send('R');rp.read(.1);rp.send('q');identity=rp.read(.3)
    expected=f'CONTINUOUS version=3 response=cpu-rom marker=preload continuous=1 words={len(rom)//2} crc={zlib.crc32(rom):08x} clock_khz=150000'
    if expected not in identity:raise p.CollectionError('candidate engine/ROM/clock identity mismatch; no arm')
    p.atomic_json(logdir/'identity.json',dict(expected=expected,received=identity))
    rp.send('b');deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        state=rp.status()
        if state['active']:raise p.CollectionError('Responder became active before held-reset rendezvous; leave service running and inspect setup')
        if not state['stopped'] and not state['arm_pending']:break
        time.sleep(.1)
    else:raise p.CollectionError('did not reach armed/reset-wait state; state not automatically changed')
    p.atomic_json(logdir/'primed.json',state)
    announce('PRIMED while RESET is held. RELEASE THE PRISM RESET NOW; do not move module/probes.',flush=True)
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        state=rp.status()
        if state['run']!=before['run']+1:raise p.CollectionError('run identity changed')
        if state['active'] and not state['stopped'] and not state['arm_pending']:
            p.atomic_json(logdir/'armed.json',state)
            announce('ACTIVE. Console will close; ROM service remains running. Do not send x or stop/reset the RP2350.',flush=True)
            return state
        time.sleep(.1)
    raise p.CollectionError('activation timeout; responder not stopped automatically')

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--manifest',type=Path,required=True);a.add_argument('--log-dir',type=Path,required=True)
    a.add_argument('--fallback',action='store_true',help='use the preserved October25 ROM/UF2 in this bundle')
    a.add_argument('--prism-reset-held',action='store_true',required=True);a.add_argument('--rp')
    args=a.parse_args();rp=None
    try:
        _,files=load_bundle(args.manifest,args.fallback)
        args.log_dir.mkdir(parents=True,exist_ok=False)
        rp=p.ContinuousRP(p.rp_port(args.rp),args.log_dir/'rp-console.log')
        prime(rp,files['rom'],args.log_dir)
    except (OSError,ValueError,KeyError,p.CollectionError,KeyboardInterrupt) as e:
        a.exit(1,f'INCOMPLETE: {e}. No automatic stop/flash/reset was performed.\n')
    finally:
        if rp:rp.close()  # close is deliberately not stop
if __name__=='__main__':main()
