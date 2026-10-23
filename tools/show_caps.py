"""Найти по логу файлы конкретных захватов и сделать из них картинки.

Номер rNN в имени файла — порядковый номер ЦВЕТНОГО resolve внутри захвата, а дамп — это
цель-источник в момент resolve. Имена повторяются между заходами, поэтому файл выбирается
по префиксу из лога этого захода и по размеру образа, который даёт текущий масштаб.
usage: python3 show_caps.py <log>
"""
import os, re, subprocess, sys
import numpy as np
import cv2

FTP = 'ftp://192.168.1.199:5000/switch/shd-nx/logs/'
OUT = '/var/minis/attachments'
WORK = '/root/shd/caps/show'
os.makedirs(WORK, exist_ok=True)

lines = open(sys.argv[1], encoding='utf-8', errors='replace').read().split('\n')
listing = subprocess.run(['curl', '-s', '--max-time', '100', FTP], capture_output=True,
                         text=True).stdout
names = sorted(set(re.findall(r'(cap\d_r\d+_\S+\.bin)', listing)))
q = 4
for l in lines:
    m = re.search(r'internal render scale (\d)/4', l)
    if m:
        q = int(m.group(1))
        break
print(f'масштаб {q}/4, файлов захватов на карте {len(names)}')

RX = re.compile(r'resolve color src fmt(\d+)@(\d+) key pitch (\d+) msaa (\d+) \(sample (\d)x(\d)\)'
                r'.*?rect (\d+),(\d+)-(\d+),(\d+) -> ([0-9A-F]{8}) tex fmt (\d+) exp (-?\d+)')


def section(n):
    s = None
    for i, l in enumerate(lines):
        if f'CAPTURE {n} armed' in l and s is None:
            s = i
        elif f'CAPTURE {n} done' in l and s is not None:
            return l, lines[s:i]
    return None, []


def pick(prefix, pitch):
    c = [n for n in names if n.startswith(prefix)]
    if len(c) <= 1:
        return (c[0] if c else None), c
    want = pitch * q // 4 if (q != 4 and pitch == 1280) else pitch
    for n in c:
        m = re.search(r'_(\d+)x(\d+)_vk\d+\.bin$', n)
        if int(m.group(1)) == min(want, 1280):
            return n, c
    return c[-1], c


def to_png(name, out, rw, rh):
    path = os.path.join(WORK, name)
    subprocess.run(['curl', '-s', '--max-time', '200', '-o', path, FTP + name])
    w, h, vk = map(int, re.search(r'_(\d+)x(\d+)_vk(\d+)\.bin$', name).groups())
    raw = open(path, 'rb').read()
    if vk == 97:
        a = np.frombuffer(raw[:w * h * 8], dtype=np.float16).astype(np.float32)
        img = np.clip(a.reshape(h, w, 4)[:, :, :3] * 255, 0, 255).astype(np.uint8)
    else:
        img = np.frombuffer(raw[:w * h * 4], dtype=np.uint8).reshape(h, w, 4)[:, :, :3]
    img = np.ascontiguousarray(img[:, :, ::-1])
    vw, vh = (rw, rh) if w >= rw else (rw * q // 4, rh * q // 4)
    img = img[:min(vh, h), :min(vw, w)]
    if img.shape[1] != rw:
        img = cv2.resize(img, (rw, rh), interpolation=cv2.INTER_LINEAR)
    cv2.imwrite(out, img)
    return w, h


for n in (1, 2, 3, 4, 5, 6):
    head, sec = section(n)
    if not sec:
        continue
    frame = re.search(r'frame (\d+)', sec[0])
    print(f'\n=== захват {n}, кадр {frame.group(1) if frame else "?"}')
    k = 0
    wanted = {}
    for l in sec:
        m = RX.search(l)
        if not m:
            continue
        k += 1
        fmt, b, pitch, msaa, sx, sy, x0, y0, x1, y1, to, tex, exp = m.groups()
        rw, rh = int(x1) - int(x0), int(y1) - int(y0)
        prefix = f'cap{n}_r{k:02d}_fmt{fmt}_b{b}_rect{rw}x{rh}_to{to}_tex{tex}_exp{exp}_'
        f, cands = pick(prefix, int(pitch))
        print(f'  r{k:02d} pitch {pitch:>4} {rw}x{rh} -> {to} exp {exp:>3} msaa {msaa} '
              f'sample {sx}x{sy} | файл {f.split("_exp")[1] if f else "НЕТ"}'
              f'{"  (вариантов " + str(len(cands)) + ")" if len(cands) > 1 else ""}')
        if to == '1F5F8000':
            wanted['final'] = (f, rw, rh)
        if to == '14830000':
            wanted['scene'] = (f, rw, rh)
    for tag, (f, rw, rh) in wanted.items():
        if f:
            out = f'{OUT}/cap{n}_{tag}.png'
            w, h = to_png(f, out, rw, rh)
            print(f'  {tag}: {f} ({w}x{h}) -> {out}')
