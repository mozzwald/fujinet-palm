#!/usr/bin/env python3
"""Generate matching Palm launcher icons in 1-bit and 8-bit BMP formats."""

from pathlib import Path
import struct


HERE = Path(__file__).resolve().parent / "icons"
HERE.mkdir(exist_ok=True)

WHITE, INK, PANEL, BODY, GOLD, GRID = range(6)
PALETTE = (
    (255, 255, 255),  # background
    (24, 41, 62),     # outlines
    (51, 102, 153),   # solar panels
    (170, 187, 187),  # body
    (255, 204, 51),   # antenna and details
    (221, 238, 255),  # panel grid
)


def canvas(width, height):
    return [[WHITE for _ in range(width)] for _ in range(height)]


def rect(img, x0, y0, x1, y1, color):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            img[y][x] = color


def large():
    img = canvas(32, 32)
    # Mast and dish above the central spacecraft body.
    rect(img, 15, 6, 16, 12, INK)
    rect(img, 14, 5, 17, 6, GOLD)
    rect(img, 12, 4, 19, 4, INK)
    rect(img, 13, 3, 18, 3, INK)
    rect(img, 10, 16, 22, 18, INK)

    # Solar arrays, with white grid lines legible at launcher scale.
    for x0 in (2, 22):
        rect(img, x0, 9, x0 + 8, 23, INK)
        rect(img, x0 + 1, 10, x0 + 7, 22, PANEL)
        rect(img, x0 + 4, 10, x0 + 4, 22, GRID)
        rect(img, x0 + 1, 14, x0 + 7, 14, GRID)
        rect(img, x0 + 1, 18, x0 + 7, 18, GRID)

    rect(img, 13, 11, 19, 23, INK)
    rect(img, 14, 12, 18, 22, BODY)
    rect(img, 15, 14, 17, 16, GOLD)
    rect(img, 15, 24, 17, 26, INK)
    rect(img, 14, 27, 18, 27, INK)
    return img


def small():
    img = canvas(15, 9)
    rect(img, 7, 0, 7, 2, INK)
    rect(img, 6, 0, 8, 0, GOLD)
    rect(img, 4, 4, 10, 4, INK)
    for x0 in (0, 10):
        rect(img, x0, 2, x0 + 4, 6, INK)
        rect(img, x0 + 1, 3, x0 + 3, 5, PANEL)
        rect(img, x0 + 2, 3, x0 + 2, 5, GRID)
    rect(img, 6, 2, 8, 7, INK)
    rect(img, 7, 3, 7, 6, BODY)
    return img


def write_bmp(path, img, color):
    height, width = len(img), len(img[0])
    depth = 8 if color else 1
    stride = ((width * depth + 31) // 32) * 4
    pixels = bytearray()
    for row in reversed(img):
        if color:
            packed = bytes(row)
        else:
            # Windows BMP palette index 0 is black, 1 is white.
            bits = [1 if px in (WHITE, GRID) else 0 for px in row]
            packed = bytes(sum(bit << (7 - i) for i, bit in enumerate(bits[j:j + 8]))
                           for j in range(0, width, 8))
        pixels.extend(packed)
        pixels.extend(b"\0" * (stride - len(packed)))

    colors = 256 if color else 2
    palette = bytearray(colors * 4)
    if color:
        for i, (red, green, blue) in enumerate(PALETTE):
            palette[i * 4:i * 4 + 4] = bytes((blue, green, red, 0))
    else:
        palette[4:8] = b"\xff\xff\xff\0"
    offset = 14 + 40 + len(palette)
    header = struct.pack("<2sIHHI", b"BM", offset + len(pixels), 0, 0, offset)
    info = struct.pack("<IiiHHIIiiII", 40, width, height, 1, depth, 0,
                       len(pixels), 3780, 3780, colors, len(PALETTE) if color else 2)
    path.write_bytes(header + info + palette + pixels)


for name, art in (("satellite_l", large()), ("satellite_s", small())):
    write_bmp(HERE / (name + ".bmp"), art, False)
    write_bmp(HERE / (name + "_color.bmp"), art, True)
