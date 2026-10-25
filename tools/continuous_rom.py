#!/usr/bin/env python3
"""Manually arm continuous ROM for a recognition session; Ctrl-C stops it.

No flashing, Palm insertion, launch, HotSync or scope stimulus is performed.
"""
import argparse
import json
from pathlib import Path
import time
import prism_collect as p


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',type=Path,required=True)
    parser.add_argument('--log-dir',type=Path,required=True)
    parser.add_argument('--rp')
    parser.add_argument('--engine',choices=('rearm','persistent'),default='rearm')
    parser.add_argument('--marker',choices=('drive','preload'),default='drive')
    parser.add_argument('--readback-session',type=Path)
    parser.add_argument('--firmware-sha256')
    args=parser.parse_args()
    args.log_dir.mkdir(parents=True,exist_ok=False)
    rp=None
    try:
        rom=args.rom.read_bytes()
        if args.engine=='persistent':
            if args.marker!='preload' or not args.readback_session or not args.firmware_sha256:
                raise p.CollectionError('persistent recognition requires preload and its completed frozen-firmware readback')
            from qualified_rom_readback import readback_gate
            p.atomic_json(args.log_dir/'readback-gate.json',readback_gate(args.readback_session,rom,args.firmware_sha256))
        rp=p.ContinuousRP(p.rp_port(args.rp),args.log_dir/'rp-console.log')
        before,config=rp.identify_continuous(rom,'cpu-rom',len(rom)//2,marker=args.marker,engine=args.engine)
        config['quiet_during_recognition']=args.engine=='persistent'
        p.atomic_json(args.log_dir/'before.json',before)
        p.atomic_json(args.log_dir/'config.json',config)
        armed=rp.arm(before,len(rom)//2)
        p.atomic_json(args.log_dir/'armed.json',armed)
        print('ACTIVE continuous ROM. Manual recognition only; do not launch the ROM app. Ctrl-C to stop.',flush=True)
        with (args.log_dir/'status.jsonl').open('x') as log:
            while True:
                time.sleep(1)
                if args.engine=='persistent':
                    # Version3 publishes totals only on stop. Keep the proven
                    # no-UART-polling load during recognition and HotSync.
                    continue
                status=rp.status()
                log.write(json.dumps(dict(status,host_monotonic=time.monotonic()))+'\n');log.flush()
                if status['run'] != armed['run'] or status['stopped'] or not status['active']:
                    raise p.CollectionError('continuous responder changed/stopped unexpectedly')
    except KeyboardInterrupt:
        print('Stopping responder.',flush=True)
    except (OSError,ValueError,p.CollectionError) as error:
        print('INCOMPLETE: '+str(error),flush=True)
        return 2
    finally:
        if rp:
            try:
                p.atomic_json(args.log_dir/'final.json',rp.stop())
                print('Stopped; final diagnostics saved.',flush=True)
            finally:
                rp.close()
    return 0


if __name__=='__main__':
    raise SystemExit(main())
