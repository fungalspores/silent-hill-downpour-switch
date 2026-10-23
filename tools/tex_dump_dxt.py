#!/usr/bin/env python3
"""Try to read a P2 texture dump (24-byte fetch + u32 size + bytes) as DXT1/DXT5 blocks, linear or Xbox-tiled,
with and without 16-bit byte swap, to see whether the bytes are a block-compressed image and not k_8.
Writes PNGs next to the dump: <name>_<fmt>_<layout>_<swap>.png (DXT5: the alpha channel, DXT1: luminance)."""
import os
import struct
import sys

from PIL import Image


def tiled_offset_2d(x, y, pitch, bpb_log2):
    pitch = (pitch + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (bpb_log2 + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << bpb_log2
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return ((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) + \
        (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F)


def swap16(b):
    out = bytearray(b)
    out[0::2], out[1::2] = b[1::2], b[0::2]
    return bytes(out)


def dxt5_alpha(block):
    a0, a1 = block[0], block[1]
    bits = int.from_bytes(block[2:8], 'little')
    if a0 > a1:
        pal = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
    else:
        pal = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
    return [pal[(bits >> (3 * i)) & 7] for i in range(16)]


def rgb565_lum(c):
    r = (c >> 11) & 31
    g = (c >> 5) & 63
    b = c & 31
    return (r * 255 // 31 * 299 + g * 255 // 63 * 587 + b * 255 // 31 * 114) // 1000


def dxt1_lum(block):
    c0, c1, bits = struct.unpack('<HHI', block[:8])
    l0, l1 = rgb565_lum(c0), rgb565_lum(c1)
    if c0 > c1:
        pal = [l0, l1, (2 * l0 + l1) // 3, (l0 + 2 * l1) // 3]
    else:
        pal = [l0, l1, (l0 + l1) // 2, 0]
    return [pal[(bits >> (2 * i)) & 3] for i in range(16)]


def decode(body, width, block_bytes, tiled, kind):
    bw, bh = width // 4, len(body) // block_bytes // (width // 4)
    img = bytearray(width * bh * 4)
    log2 = 4 if block_bytes == 16 else 3
    for by in range(bh):
        for bx in range(bw):
            off = tiled_offset_2d(bx, by, bw, log2) if tiled else (by * bw + bx) * block_bytes
            blk = body[off:off + block_bytes]
            if len(blk) < block_bytes:
                continue
            texels = dxt5_alpha(blk) if kind == 'dxt5a' else dxt1_lum(blk[8:] if block_bytes == 16 else blk)
            for i, v in enumerate(texels):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                img[y * width + x] = v
    return Image.frombytes('L', (width, bh * 4), bytes(img))


def main():
    path = sys.argv[1]
    data = open(path, 'rb').read()
    size = struct.unpack('<I', data[24:28])[0]
    body = data[28:28 + size]
    name = os.path.splitext(os.path.basename(path))[0]
    out = sys.argv[2] if len(sys.argv) > 2 else '.'
    for swap in (False, True):
        b = swap16(body) if swap else body
        for tiled in (False, True):
            for kind, block_bytes, width in (('dxt5a', 16, 1024), ('dxt5c', 16, 1024), ('dxt1', 8, 1024)):
                im = decode(b, width, block_bytes, tiled, kind)
                fn = '%s_%s_%s_%s.png' % (name, kind, 'tiled' if tiled else 'linear', 'swap' if swap else 'noswap')
                im.save(os.path.join(out, fn))
                print('wrote', fn, im.size)


if __name__ == '__main__':
    main()
