#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
./docker/palm-build.sh true
docker build --platform linux/amd64 -t fujinet-palm-springboard-tools -f docker/Dockerfile.springboard .
PALM_IMAGE=fujinet-palm-springboard-tools ./docker/palm-build.sh make -C palm/apps/visorbus rom
PALM_IMAGE=fujinet-palm-springboard-tools ./docker/palm-build.sh make -C palm/apps/visorcheck
python3 tools/tests/test_springboard_rom.py
