#!/usr/bin/env python3
"""Locate the XDK Direct3D entry points Silent Hill: Downpour uses, from the dumped image.

Heuristics (the device object layout is the same XDK as Mass Effect's, see masseffect-nx docs/native-renderer.md):
  * device register shadow: dev+10548 RB_DEPTHCONTROL, +10552 RB_BLENDCONTROL0, +10556 RB_COLORCONTROL,
    +10560 PA_SU_SC_MODE_CNTL, +10500 ALPHA_REF;
  * the bound shader objects live in the device; the setter pair stores to one offset and the getter loads it.
  * draws write the PM4 packet: they are the functions that store a word with the draw-initiator opcode into the
    command buffer and bump the device's write pointer.

    find_d3d.py <image> <generated dir> [what]
"""
import bisect
import collections
import glob
import os
import re
import struct
import sys

BASE = 0x82000000


def load(image):
    return open(image, 'rb').read()


def funcs(gen):
    out = set()
    for p in glob.glob(os.path.join(gen, '*_recomp.*.cpp')):
        for m in re.finditer(r'DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\)', open(p).read()):
            out.add(int(m.group(1), 16))
    return sorted(out)


class Img:
    def __init__(self, path, gen):
        self.d = load(path)
        self.starts = funcs(gen)

    def w(self, a):
        return struct.unpack_from('>I', self.d, a - BASE)[0]

    def owner(self, a):
        i = bisect.bisect_right(self.starts, a) - 1
        return self.starts[i] if i >= 0 else None

    def code_range(self):
        return 0x82300000, 0x8333023C


def stores_to(img, offset, ops=(36, 44, 38, 32, 40, 34, 14)):
    """Instructions with this displacement; returns {function: [pc]}."""
    lo, hi = img.code_range()
    out = collections.defaultdict(list)
    for pc in range(lo, hi, 4):
        w = img.w(pc)
        if (w >> 26) in ops and (w & 0xFFFF) == offset:
            out[img.owner(pc)].append(pc)
    return out


def main():
    img = Img(sys.argv[1], sys.argv[2])
    what = sys.argv[3] if len(sys.argv) > 3 else 'regs'
    if what == 'regs':
        for off, name in ((10500, 'ALPHA_REF'), (10548, 'RB_DEPTHCONTROL'), (10552, 'RB_BLENDCONTROL0'),
                          (10556, 'RB_COLORCONTROL'), (10560, 'PA_SU_SC_MODE_CNTL')):
            d = stores_to(img, off)
            print(f'{name} (dev+{off}): {len(d)} functions')
            for fn, pcs in sorted(d.items(), key=lambda kv: -len(kv[1]))[:6]:
                print(f'   sub_{fn:08X}  {len(pcs)} sites')
    elif what == 'offset':
        off = int(sys.argv[4], 0)
        for fn, pcs in sorted(stores_to(img, off).items()):
            print(f'sub_{fn:08X}  ' + ' '.join(f'{p:08X}' for p in pcs[:8]))


if __name__ == '__main__':
    main()
