#!/usr/bin/env python3
"""Build the imported 32-bit MakeROM with the container's Palm OS SDK."""
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parent
sdk = Path('/opt/palmdev/sdk-3.5/include')
out = root / 'build'
out.mkdir(exist_ok=True)
includes = ['-I' + str(sdk), '-I' + str(root)]
includes += ['-I' + str(p) for p in sorted(sdk.rglob('*')) if p.is_dir()]
subprocess.run(['g++', '-m32', '-std=gnu++98', '-fpermissive',
    '-DCPU_TYPE=1', '-DEMULATION_LEVEL=1', '-D_strdup=strdup',
    *includes, *[str(root / p) for p in ['Main.cpp', 'Utility.cpp', 'Crc.cpp']],
    '-o', str(out / 'Palm-MakeROM')], check=True)
