#!/usr/bin/env python3
"""Write the PNG files dmimg_png_test.c decodes - with a PNG writer of its
own (zlib only), as no common library writes interlaced PNGs.

Every image is 9 x 7 of the pattern dmimg_png_test.c checks:
    R = x * 28, G = y * 36, B = (x * y * 5) & 0xFF, A = 255 - x * 20
"""

import struct
import zlib

W, H = 9, 7


def pattern(x, y):
    return (x * 28, y * 36, (x * y * 5) & 0xFF, 255 - x * 20)


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF)


def scanlines(rows):
    return b''.join(b'\0' + bytes(r) for r in rows)      # filter 0 - the decoder's filters are pngle's


def adam7(pixel_bytes):
    passes = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]
    out = b''
    for x0, y0, dx, dy in passes:
        rows = [[b for x in range(x0, W, dx) for b in pixel_bytes(x, y)] for y in range(y0, H, dy)]
        if rows and rows[0]:
            out += scanlines(rows)
    return out


def png(name, color_type, depth, pixel_bytes, interlace=0, extra=b''):
    if interlace:
        raw = adam7(pixel_bytes)
    else:
        raw = scanlines([[b for x in range(W) for b in pixel_bytes(x, y)] for y in range(H)])
    data = b'\x89PNG\r\n\x1a\n'
    data += chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, depth, color_type, 0, 0, interlace))
    data += extra
    data += chunk(b'IDAT', zlib.compress(raw, 9))
    data += chunk(b'IEND', b'')
    with open(name, 'wb') as f:
        f.write(data)


def rgb(x, y):
    return pattern(x, y)[:3]


def rgba(x, y):
    return pattern(x, y)


def rgb16(x, y):
    return b''.join(struct.pack('>H', v * 257) for v in pattern(x, y)[:3])


png('rgb.png', 2, 8, rgb)
png('rgba.png', 6, 8, rgba)
png('rgb16.png', 2, 16, rgb16)
png('adam7.png', 6, 8, rgba, interlace=1)

# A palette of the colors of row 0, and tRNS with their alpha
palette = b''.join(bytes(pattern(x, 0)[:3]) for x in range(W))
trns = bytes(pattern(x, 0)[3] for x in range(W))
png('palette.png', 3, 8, lambda x, y: (x,), extra=chunk(b'PLTE', palette) + chunk(b'tRNS', trns))
