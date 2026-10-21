#!/usr/bin/env python3
"""Generate the RAM diagnostic's expected CRC from the verified module image."""
import argparse
import hashlib
import json
from pathlib import Path
import zlib

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('rom', type=Path)
p.add_argument('manifest', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
data = a.rom.read_bytes()
manifest = json.loads(a.manifest.read_text())
if (not 512 <= len(data) <= 65536 or len(data) % 2 or
    manifest.get('size') != len(data) or
    manifest.get('sha256') != hashlib.sha256(data).hexdigest()):
    p.error('ROM does not match verified manifest or permitted length')
a.output.write_text('/* Generated from verified module ROM, not this diagnostic. */\n'
    '#define EXPECTED_ROM_SIZE %dUL\n#define EXPECTED_ROM_CRC 0x%08xUL\n' %
    (len(data), zlib.crc32(data)) +
    'static const unsigned char expected_rom_data[] = {\n' +
    ',\n'.join(','.join('0x%02x' % b for b in data[i:i+16])
              for i in range(0, len(data), 16)) + '\n};\n')
