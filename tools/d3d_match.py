#!/usr/bin/env python3
"""Find the XDK Direct3D entry points of one game by matching them against another game's, by code shape.

Both games statically link the same Xbox 360 Direct3D, so the instruction opcodes of an entry point are nearly
identical; only the embedded addresses differ. A function is reduced to a "shape": the opcode fields of every
instruction with the address-bearing bits masked out. Shapes are then matched between the two images.

    d3d_match.py <image A> <addresses A, hex, comma separated> <image B>
"""
import struct
import sys

BASE = 0x82000000


class Image:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        pe = struct.unpack_from('<I', self.d, 0x3C)[0]
        count = struct.unpack_from('<H', self.d, pe + 6)[0]
        opt = struct.unpack_from('<H', self.d, pe + 20)[0]
        self.code = []
        for i in range(count):
            s = pe + 24 + opt + i * 40
            vsize, va = struct.unpack_from('<II', self.d, s + 8)
            flags = struct.unpack_from('<I', self.d, s + 36)[0]
            if flags & 0x20:
                self.code.append((BASE + va, BASE + va + vsize))

    def word(self, a):
        return struct.unpack_from('>I', self.d, a - BASE)[0]

    def in_code(self, a):
        return any(lo <= a < hi for lo, hi in self.code)


def mask(w):
    """Opcode and register fields, without displacements / immediates / branch targets."""
    op = w >> 26
    if op in (16, 18):            # branches: keep only the opcode and the link/absolute bits
        return (op << 26) | (w & 3)
    if op in (14, 15, 24, 25, 26, 27, 28, 29) or 32 <= op <= 55:  # addi/addis/ori/loads/stores: drop the immediate
        return w & 0xFFFF0000
    if op == 31:                  # extended: opcode + subopcode + registers
        return w & 0xFFFFF801
    if op == 19:
        return w & 0xFFFFF801
    return w


def shape(img, addr, limit=400):
    out = []
    a = addr
    while a < addr + limit * 4 and img.in_code(a):
        w = img.word(a)
        out.append(mask(w))
        if w == 0x4E800020:       # blr
            break
        a += 4
    return tuple(out)


def main():
    a = Image(sys.argv[1])
    addrs = [int(x, 16) for x in sys.argv[2].replace('0x', '').split(',')]
    b = Image(sys.argv[3])
    # Every aligned address in B's code is a candidate start.
    index = {}
    for lo, hi in b.code:
        for pc in range(lo, hi - 3, 4):
            w = b.word(pc)
            op = w >> 26
            if op not in (31, 32, 36, 37, 14, 15, 7, 11, 18, 16, 19, 24, 40, 44, 33, 38, 34, 35, 20, 21, 10):
                continue
            index.setdefault(mask(w), []).append(pc)
    for addr in addrs:
        s = shape(a, addr)
        if len(s) < 6:
            print(f'{addr:08X}: too short')
            continue
        best = []
        for pc in index.get(s[0], ()):
            t = shape(b, pc, len(s) + 8)
            n = min(len(s), len(t))
            same = sum(1 for i in range(n) if s[i] == t[i])
            score = same / max(len(s), len(t))
            if score > 0.9:
                best.append((score, pc, len(t)))
        best.sort(reverse=True)
        if not best:
            print(f'{addr:08X} ({len(s)} insns): no match')
        else:
            tops = ', '.join(f'{pc:08X} {sc:.3f}' for sc, pc, _ in best[:3])
            print(f'{addr:08X} ({len(s)} insns): {tops}' + ('' if len(best) == 1 else f'  [{len(best)} candidates]'))


if __name__ == '__main__':
    main()
