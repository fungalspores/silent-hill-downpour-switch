#!/usr/bin/env python3
"""Silent Hill: Downpour UE3 package reader (version 756, Xbox 360, big-endian, LZX chunks).

Rebuilds the uncompressed package image (header + chunks at their uncompressed offsets) with the C++ LZX helper
(tools/lzxblock), parses names/imports/exports and can dump export data.

    ue3pkg.py <pkg> exports [filter]     list exports (class, name, offset, size)
    ue3pkg.py <pkg> dump <name> <out>    write one export's data
    ue3pkg.py <pkg> image <out>          write the whole uncompressed image
"""
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LZX = os.path.join(HERE, 'lzxblock')


class R:
    def __init__(self, d, o=0):
        self.d, self.o = d, o

    def u32(self):
        v = struct.unpack_from('>I', self.d, self.o)[0]
        self.o += 4
        return v

    def i32(self):
        v = struct.unpack_from('>i', self.d, self.o)[0]
        self.o += 4
        return v

    def u64(self):
        v = struct.unpack_from('>Q', self.d, self.o)[0]
        self.o += 8
        return v

    def fstr(self):
        n = self.i32()
        if n == 0:
            return ''
        if n < 0:
            s = self.d[self.o:self.o - 2 * n].decode('utf-16-be', 'replace')
            self.o += -2 * n
        else:
            s = self.d[self.o:self.o + n].decode('latin1')
            self.o += n
        return s.rstrip('\0')


def decompress_chunk(d, off, size):
    tag, bs, c, u = struct.unpack_from('>IIII', d, off)
    assert tag == 0x9E2A83C1, hex(tag)
    n = (u + bs - 1) // bs
    blocks = [struct.unpack_from('>II', d, off + 16 + k * 8) for k in range(n)]
    p = off + 16 + n * 8
    out = bytearray()
    for bc, bu in blocks:
        if bc == bu:
            out += d[p:p + bc]
        else:
            r = subprocess.run([LZX, str(bu)], input=d[p:p + bc], capture_output=True)
            if r.returncode != 0 or len(r.stdout) != bu:
                raise RuntimeError(f'LZX block at {p:#x} failed ({r.returncode})')
            out += r.stdout
        p += bc
    return bytes(out)


def load(path):
    d = open(path, 'rb').read()
    tag, bs = struct.unpack_from('>II', d, 0)
    if bs == 0x20000:  # the whole file is one compressed chunk (*.uncpsd_sz next to it gives the size)
        d = decompress_chunk(d, 0, len(d))
    r = R(d, 4)
    ver = r.u32()
    header_size = r.u32()
    folder = r.fstr()
    flags = r.u32()
    name_count, name_off = r.u32(), r.u32()
    export_count, export_off = r.u32(), r.u32()
    import_count, import_off = r.u32(), r.u32()
    depends_off = r.u32()
    if (ver & 0xFFFF) >= 623:
        r.u32(); r.u32(); r.u32()
    if (ver & 0xFFFF) >= 584:
        r.u32()
    r.o += 16  # guid
    gens = r.u32()
    r.o += gens * 12
    r.u32(); r.u32()  # engine, cooker version
    comp = r.u32()
    nchunks = r.u32()
    chunks = [struct.unpack_from('>IIII', d, r.o + k * 16) for k in range(nchunks)]
    if chunks:
        size = max(uo + us for uo, us, co, cs in chunks)
        img = bytearray(max(size, header_size))
        img[:header_size] = d[:header_size]
        for uo, us, co, cs in chunks:
            img[uo:uo + us] = decompress_chunk(d, co, cs)[:us]
        d = bytes(img)
    return d, ver, (name_count, name_off), (export_count, export_off), (import_count, import_off)


def parse(path):
    d, ver, names_t, exports_t, imports_t = load(path)
    r = R(d, names_t[1])
    names = []
    for _ in range(names_t[0]):
        names.append(r.fstr())
        r.u64()  # flags
    def nm(idx, num):
        s = names[idx] if 0 <= idx < len(names) else f'?{idx}'
        return s if num == 0 else f'{s}_{num - 1}'
    r = R(d, imports_t[1])
    imports = []
    for _ in range(imports_t[0]):
        cp, cpn, cn, cnn, outer, on, onn = r.u32(), r.u32(), r.u32(), r.u32(), r.i32(), r.u32(), r.u32()
        imports.append((nm(cn, cnn), nm(on, onn)))
    def objname(i):
        if i < 0:
            return imports[-i - 1][1]
        if i > 0:
            return exports[i - 1]['name']
        return 'Class'
    r = R(d, exports_t[1])
    exports = []
    for _ in range(exports_t[0]):
        cls, sup, outer = r.i32(), r.i32(), r.i32()
        on, onn = r.u32(), r.u32()
        arch = r.i32()
        oflags = r.u64()
        size, off = r.u32(), r.u32()
        # v756 (SHD): no component map; export flags, generation net counts, package guid, package flags
        r.u32()
        ngen = r.u32()
        r.o += ngen * 4
        r.o += 16
        r.u32()
        exports.append({'cls': cls, 'name': nm(on, onn), 'size': size, 'off': off})
    for e in exports:
        e['class'] = objname(e['cls'])
    return d, exports


def main():
    path, cmd = sys.argv[1], sys.argv[2]
    d, exports = parse(path)
    if cmd == 'exports':
        flt = sys.argv[3].lower() if len(sys.argv) > 3 else ''
        for e in exports:
            if flt in (e['class'] + ' ' + e['name']).lower():
                print(f"{e['class']:32} {e['name']:48} off {e['off']:#x} size {e['size']}")
    elif cmd == 'dump':
        e = next(e for e in exports if e['name'] == sys.argv[3])
        open(sys.argv[4], 'wb').write(d[e['off']:e['off'] + e['size']])
    elif cmd == 'image':
        open(sys.argv[3], 'wb').write(d)


if __name__ == '__main__':
    main()
