"""Разбирает альфа-блоки BC3/DXT5 в сыром куске данных и показывает,
насколько «бинарной» выходит альфа при разном порядке байт.
python3 bc3_check.py <файл> [смещение] [сколько_блоков]"""
import collections
import sys


def alphas(block):
    """16 значений альфы из 8-байтового альфа-блока BC3."""
    a0, a1 = block[0], block[1]
    bits = int.from_bytes(block[2:8], "little")
    if a0 > a1:
        table = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
    else:
        table = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
    return [table[(bits >> (3 * i)) & 7] for i in range(16)]


def swap(data, unit):
    if unit == 1:
        return data
    out = bytearray(data)
    for i in range(0, len(out) - unit + 1, unit):
        out[i:i + unit] = out[i:i + unit][::-1]
    return bytes(out)


def score(data, blocks):
    """Доля значений альфы, которые ровно 0 или 255 (признак сломанного декода)."""
    hard = total = 0
    hist = collections.Counter()
    for b in range(blocks):
        blk = data[b * 16:b * 16 + 8]
        if len(blk) < 8:
            break
        for v in alphas(blk):
            total += 1
            hist[v // 32] += 1
            if v in (0, 255):
                hard += 1
    return (hard / total if total else 0), total, hist


path = sys.argv[1]
off = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0
nblocks = int(sys.argv[3]) if len(sys.argv) > 3 else 4096
raw = open(path, "rb").read()[off:off + nblocks * 16]
for unit, label in ((1, "как есть"), (2, "обмен по 2 байта"), (4, "обмен по 4 байта")):
    frac, total, hist = score(swap(raw, unit), nblocks)
    bars = " ".join(f"{k * 32}-{k * 32 + 31}:{v}" for k, v in sorted(hist.items()))
    print(f"{label:18} крайних 0/255: {frac * 100:5.1f}%  из {total}  | {bars}")
