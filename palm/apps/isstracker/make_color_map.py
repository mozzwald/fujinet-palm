#!/usr/bin/env python3
"""Colorize the existing 1-bit, 160x80 ISS map without changing its mask."""

from pathlib import Path
import struct


HERE = Path(__file__).resolve().parent
source = (HERE / "map.bmp").read_bytes()
pixel_offset = struct.unpack_from("<I", source, 10)[0]
width, height = struct.unpack_from("<ii", source, 18)
depth = struct.unpack_from("<H", source, 28)[0]
assert (width, height, depth) == (160, 80, 1)

source_stride = ((width + 31) // 32) * 4
pixels = bytearray()
for row in range(height):
    row_start = pixel_offset + row * source_stride
    for x in range(width):
        byte = source[row_start + x // 8]
        is_water = (byte >> (7 - x % 8)) & 1
        pixels.append(0 if is_water else 1)

# BMP palette entries are blue, green, red, reserved. Use a small palette
# with an attached Palm color table so PilRC preserves these colors.
palette = bytearray(256 * 4)
palette[0:4] = bytes((0x99, 0x66, 0x33, 0))  # ocean: #336699
palette[4:8] = bytes((0x55, 0xAA, 0x66, 0))  # land:  #66AA55
offset = 14 + 40 + len(palette)
size = offset + len(pixels)
header = struct.pack("<2sIHHI", b"BM", size, 0, 0, offset)
info = struct.pack(
    "<IiiHHIIiiII", 40, width, height, 1, 8, 0, len(pixels),
    3780, 3780, 256, 2,
)
(HERE / "map_color.bmp").write_bytes(header + info + palette + pixels)
