#!/usr/bin/env python3
"""Separate four-pair MOVE.W/NOP collector; reuses serial palm-sync workflow."""
import paired_collect as collector
if __name__=='__main__':
    collector.WORD_MODE=True
    try:raise SystemExit(collector.main())
    except (OSError,ValueError,collector.p.CollectionError) as error:
        print('INCOMPLETE: '+str(error),flush=True);raise SystemExit(2)
