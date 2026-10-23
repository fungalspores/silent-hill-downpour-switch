#!/usr/bin/env python3
"""Decode the mip chain of a texture from its dumped mip page the way the renderer does, level by level,
so a level that lands on the wrong bytes is visible. DXT1/DXT5 only (formats 18 / 20).

Usage: mip_check.py <mip page .bin> <base width> <base height> <format 18|20> [out dir]
Writes level_NN.png for every level stored in the page, and prints the layout numbers.
"""
import math
import os
import struct
import sys

import numpy as np
from PIL import Image


def align(v, a):
    return (v + a - 1) // a * a


def next_pow2(v):
    return 1 << (v - 1).bit_length()


def tiled_offset_2d(x, y, pitch, bpb_log2):
    pitch = (pitch + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (bpb_log2 + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << bpb_log2
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return ((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) + \
        (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F)


def packed_mip_level(w, h):
    l = min(math.ceil(math.log2(w)), math.ceil(math.log2(h)))
    return max(l - 4, 0)


def packed_mip_offset(w, h, mip):
    """GetPackedMipOffset from the SDK, in blocks of 4x4 (DXT)."""
    log2_w, log2_h = math.ceil(math.log2(w)), math.ceil(math.log2(h))
    log2_size = min(log2_w, log2_h)
    if log2_size > 4 + mip:
        return 0, 0
    base = max(log2_size - 4, 0)
    packed = mip - base
    if packed < 3:
        if log2_w > log2_h:
            x, y = 0, 16 >> packed
        else:
            x, y = 16 >> packed, 0
    else:
        if log2_w > log2_h:
            x, y = (1 << (log2_w - base)) >> (packed - 2), 0
        else:
            x, y = 0, (1 << (log2_h - base)) >> (packed - 2)
    return x // 4, y // 4


def rgb565(c):
    return np.array([((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31],
                    dtype=np.int32)


def dxt_color(block8):
    c0, c1 = struct.unpack('<HH', block8[:4])
    bits = struct.unpack('<I', block8[4:8])[0]
    p0, p1 = rgb565(c0), rgb565(c1)
    pal = [p0, p1, (2 * p0 + p1) // 3, (p0 + 2 * p1) // 3] if c0 > c1 else \
          [p0, p1, (p0 + p1) // 2, np.zeros(3, dtype=np.int32)]
    return np.array([pal[(bits >> (2 * i)) & 3] for i in range(16)], dtype=np.uint8).reshape(4, 4, 3)


def main():
    path, bw, bh, fmt = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
    out = sys.argv[5] if len(sys.argv) > 5 else '.'
    os.makedirs(out, exist_ok=True)
    raw = open(path, 'rb').read()
    size = struct.unpack('<I', raw[24:28])[0]
    body = bytearray(raw[28:28 + size])
    body[0::2], body[1::2] = raw[29:28 + size:2], raw[28:28 + size:2]  # 8in16 swap
    body = bytes(body)
    bbytes = 8 if fmt == 18 else 16
    log2 = 3 if fmt == 18 else 4
    level_packed = packed_mip_level(bw, bh)
    print('base %dx%d fmt %d: packing starts at level %d, page %d bytes' % (bw, bh, fmt, level_packed, size))
    offset = 0
    for level in range(1, 12):
        w, h = max(bw >> level, 1), max(bh >> level, 1)
        if w < 1 or h < 1:
            break
        stored = min(level, level_packed)
        # Row pitch of the stored level, in blocks, tile aligned (SDK GetGuestTextureLayout).
        pitch_texels = max(next_pow2(bw) >> stored, 1)
        pitch_blocks = align(align(pitch_texels, 4) // 4, 32)
        rows_texels = max(next_pow2(bh) >> stored, 1)
        rows_blocks = align(align(rows_texels, 4) // 4, 32)
        if level == stored:
            offset_level = offset
            offset += pitch_blocks * bbytes * rows_blocks
        else:
            offset_level = None  # inside the packed tail of `stored`
        ox, oy = packed_mip_offset(bw, bh, level) if level >= level_packed else (0, 0)
        src_base = (offset_level if offset_level is not None else
                    sum(align(align(max(next_pow2(bw) >> s, 1), 4) // 4, 32) * bbytes *
                        align(align(max(next_pow2(bh) >> s, 1), 4) // 4, 32)
                        for s in range(1, min(level, level_packed))))
        blocks_x, blocks_y = max(w // 4, 1), max(h // 4, 1)
        img = np.zeros((blocks_y * 4, blocks_x * 4, 3), dtype=np.uint8)
        for by in range(blocks_y):
            for bx in range(blocks_x):
                o = src_base + tiled_offset_2d(bx + ox, by + oy, pitch_blocks, log2)
                blk = body[o:o + bbytes]
                if len(blk) < bbytes:
                    continue
                img[by * 4:by * 4 + 4, bx * 4:bx * 4 + 4] = dxt_color(blk[8:] if bbytes == 16 else blk)
        Image.fromarray(img).save(os.path.join(out, 'level_%02d.png' % level))
        print('  level %2d %4dx%-4d stored %2d pitch %3d blocks offset %7d corner (%d,%d)'
              % (level, w, h, stored, pitch_blocks, src_base, ox, oy))
        if w <= 4 or h <= 4:
            break


if __name__ == '__main__':
    main()
