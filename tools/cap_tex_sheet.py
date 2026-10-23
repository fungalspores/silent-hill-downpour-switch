#!/usr/bin/env python3
"""Decode every capN_tex_<addr>_f<fmt>_<w>x<h>.bin (24-byte fetch + u32 size + guest bytes) the way the GPU
should: untile (Xbox 2D tiling), byte swap by the fetch endianness, decode DXT1/DXT3/DXT5/8888/k_8.
Writes one PNG per texture and a contact sheet with labels, to spot the one that comes out as noise.

Usage: cap_tex_sheet.py <dir with bins> <out dir> [sheet.jpg]
"""
import glob
import os
import re
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw


def tiled_offset_2d(x, y, pitch, bpb_log2):
    pitch = (pitch + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (bpb_log2 + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << bpb_log2
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return ((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) + \
        (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F)


def swap(data, endian):
    a = np.frombuffer(data, dtype=np.uint8).copy()
    n = len(a) // 4 * 4
    if endian == 1:  # 8in16
        a[:n // 2 * 2] = a[:n // 2 * 2].reshape(-1, 2)[:, ::-1].reshape(-1)
    elif endian == 2:  # 8in32
        a[:n] = a[:n].reshape(-1, 4)[:, ::-1].reshape(-1)
    elif endian == 3:  # 16in32
        a[:n] = a[:n].reshape(-1, 2, 2)[:, ::-1, :].reshape(-1)
    return a.tobytes()


def rgb565(c):
    return np.stack([((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31], -1)


def dxt_color(block8):
    c0, c1 = struct.unpack('<HH', block8[:4])
    bits = struct.unpack('<I', block8[4:8])[0]
    p0, p1 = rgb565(np.array(c0)), rgb565(np.array(c1))
    if c0 > c1:
        pal = [p0, p1, (2 * p0 + p1) // 3, (p0 + 2 * p1) // 3]
    else:
        pal = [p0, p1, (p0 + p1) // 2, np.zeros(3, dtype=p0.dtype)]
    return np.array([pal[(bits >> (2 * i)) & 3] for i in range(16)], dtype=np.uint8).reshape(4, 4, 3)


def decode(path):
    raw = open(path, 'rb').read()
    fetch = struct.unpack('<6I', raw[:24])
    size = struct.unpack('<I', raw[24:28])[0]
    body = raw[28:28 + size]
    m = re.search(r'_f(\d+)_(\d+)x(\d+)\.bin$', path)
    fmt, w, h = int(m.group(1)), int(m.group(2)), int(m.group(3))
    d0, d1 = fetch[0], fetch[1]
    tiled = d0 >> 31
    pitch = ((d0 >> 22) & 0x1FF) * 32
    endian = (d1 >> 6) & 3
    body = swap(body, endian)
    block = {18: (4, 8), 19: (4, 16), 20: (4, 16), 6: (1, 4), 2: (1, 1)}.get(fmt)
    if not block:
        return None, (fmt, w, h, tiled, pitch, endian)
    bdim, bbytes = block
    bw, bh = (w + bdim - 1) // bdim, (h + bdim - 1) // bdim
    pitch_blocks = max(pitch // bdim, bw)
    log2 = {1: 0, 4: 2, 8: 3, 16: 4}[bbytes]
    img = np.zeros((bh * bdim, bw * bdim, 3), dtype=np.uint8)
    for by in range(bh):
        for bx in range(bw):
            off = tiled_offset_2d(bx, by, pitch_blocks, log2) if tiled else (by * pitch_blocks + bx) * bbytes
            blk = body[off:off + bbytes]
            if len(blk) < bbytes:
                continue
            if fmt == 18:
                img[by * 4:by * 4 + 4, bx * 4:bx * 4 + 4] = dxt_color(blk)
            elif fmt in (19, 20):
                img[by * 4:by * 4 + 4, bx * 4:bx * 4 + 4] = dxt_color(blk[8:])
            elif fmt == 6:
                img[by, bx] = (blk[1], blk[2], blk[3])  # ARGB in memory after swap
            elif fmt == 2:
                img[by, bx] = (blk[0], blk[0], blk[0])
    return Image.fromarray(img[:h, :w]), (fmt, w, h, tiled, pitch, endian)


def main():
    src, out = sys.argv[1], sys.argv[2]
    sheet_path = sys.argv[3] if len(sys.argv) > 3 else os.path.join(out, 'sheet.jpg')
    os.makedirs(out, exist_ok=True)
    files = sorted(glob.glob(os.path.join(src, 'cap*_tex_*.bin')))
    thumbs = []
    for f in files:
        im, info = decode(f)
        name = os.path.basename(f).replace('.bin', '')
        if im is None:
            print('skip', name, info)
            continue
        im.save(os.path.join(out, name + '.png'))
        t = im.copy()
        t.thumbnail((128, 128))
        thumbs.append((name.split('_tex_')[1], t))
    cols = 8
    rows = (len(thumbs) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * 132, rows * 146), (40, 40, 40))
    dr = ImageDraw.Draw(sheet)
    for i, (n, t) in enumerate(thumbs):
        x, y = (i % cols) * 132, (i // cols) * 146
        sheet.paste(t, (x + 2, y + 14))
        dr.text((x + 2, y + 1), n[:22], fill=(255, 255, 0))
    sheet.save(sheet_path, quality=85)
    print('decoded', len(thumbs), 'of', len(files), '->', sheet_path)


if __name__ == '__main__':
    main()
