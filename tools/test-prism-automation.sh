#!/bin/sh
# Offline-only: never opens the Prism, RP2350 or instruments.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
make -C palm/apps/visorcheck check
python3 -m unittest discover -s tools/tests -p test_prism_collect.py -v
