#!/usr/bin/env python3
"""Decode a P2 texture dump (logs/tex_<addr>_NN.bin: 24-byte fetch constant + u32 size + guest bytes)
into PNGs, several ways, to see which decoding gives readable glyphs.

Usage: tex_dump_view.py dump.bin [width height] [out_dir]
Writes <name>_linear.png, <name>_tiled.png and, for 8 bpp, <name>_linear_w<pitch>.png variants.
"""
import struct
import sys
import os
from PIL import Image


def tiled_offset_2d(x, y, pitch, bpb_log2):
    # Same as GetTiledOffset2D in the SDK (src/graphics/pipeline/texture/util.cpp).
    pitch = (pitch + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (bpb_log2 + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << bpb_log2
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return ((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) + \
        (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F)


def main():
    path = sys.argv[1]
    data = open(path, 'rb').read()
    fetch = struct.unpack('>6I', data[:24]) if False else struct.unpack('<6I', data[:24])
    size = struct.unpack('<I', data[24:28])[0]
    body = data[28:28 + size]
    w = int(sys.argv[2]) if len(sys.argv) > 3 else 1024
    h = int(sys.argv[3]) if len(sys.argv) > 3 else 1024
    out = sys.argv[4] if len(sys.argv) > 4 else os.path.dirname(path) or '.'
    name = os.path.splitext(os.path.basename(path))[0]
    print('fetch words (LE):', ' '.join('%08X' % v for v in fetch), 'bytes', size, 'body', len(body))
    nonzero = sum(1 for b in body if b)
    print('non-zero bytes: %d (%.1f%%)' % (nonzero, 100.0 * nonzero / max(1, len(body))))
    # 8 bits per texel (k_8): linear rows of `w` bytes.
    img = Image.frombytes('L', (w, min(h, len(body) // w)), body[:w * min(h, len(body) // w)])
    img.save(os.path.join(out, name + '_linear.png'))
    # Xbox 2D tiling, 1 byte per texel.
    tiled = bytearray(w * h)
    for y in range(h):
        for x in range(w):
            o = tiled_offset_2d(x, y, w, 0)
            if o < len(body):
                tiled[y * w + x] = body[o]
    Image.frombytes('L', (w, h), bytes(tiled)).save(os.path.join(out, name + '_tiled.png'))
    # Byte order inside 16/32-bit words (endian 1/2): swap and show linear again.
    sw16 = bytearray(body)
    sw16[0::2], sw16[1::2] = body[1::2], body[0::2]
    Image.frombytes('L', (w, len(sw16) // w), bytes(sw16[:w * (len(sw16) // w)])).save(
        os.path.join(out, name + '_linear_swap16.png'))
    print('wrote', name + '_linear.png', name + '_tiled.png', name + '_linear_swap16.png')


if __name__ == '__main__':
    main()
