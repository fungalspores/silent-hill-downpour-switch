#!/usr/bin/env python3
"""Extract an Xbox 360 XDVDFS (XGD2/XGD3 or trimmed xiso) image."""
import os, struct, sys

SECTOR = 2048
MAGIC = b"MICROSOFT*XBOX*MEDIA"
OFFSETS = [0, 0xFD90000, 0x2080000, 0x18300000]


def main(iso, out):
    f = open(iso, "rb")
    base = None
    for o in OFFSETS:
        f.seek(o + 32 * SECTOR)
        if f.read(20) == MAGIC:
            base = o
            break
    if base is None:
        sys.exit("no XDVDFS header")
    f.seek(base + 32 * SECTOR + 20)
    root_sec, root_size = struct.unpack("<II", f.read(8))
    print(f"partition offset {base:#x}, root sector {root_sec}, size {root_size}")
    total = [0, 0]

    def walk(sec, size, path):
        if size == 0:
            return
        f.seek(base + sec * SECTOR)
        data = f.read(size)
        stack, seen = [0], set()
        while stack:
            off = stack.pop()
            if off in seen or off + 14 > len(data):
                continue
            seen.add(off)
            left, right, s, sz, attr, nlen = struct.unpack_from("<HHIIBB", data, off)
            if left == 0xFFFF:
                continue
            name = data[off + 14: off + 14 + nlen].decode("latin1")
            if left:
                stack.append(left * 4)
            if right:
                stack.append(right * 4)
            p = os.path.join(path, name)
            if attr & 0x10:
                os.makedirs(p, exist_ok=True)
                walk(s, sz, p)
            else:
                with open(p, "wb") as o:
                    f.seek(base + s * SECTOR)
                    rem = sz
                    while rem:
                        chunk = f.read(min(rem, 8 << 20))
                        o.write(chunk)
                        rem -= len(chunk)
                total[0] += 1
                total[1] += sz

    os.makedirs(out, exist_ok=True)
    walk(root_sec, root_size, out)
    print(f"{total[0]} files, {total[1] / 2**30:.2f} GiB")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
