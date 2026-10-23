"""Детектор повтора кадра в левой верхней четверти — по КОНТУРАМ, а не по яркости.

Яркостная корреляция ловит похожесть самой сцены (небо сверху, земля снизу) и одинакова
с повтором и без. Контуры силуэтов совпадают по месту только при настоящей копии.
Для каждой четверти: корреляция её градиентов с градиентами уменьшенного вдвое образа.
Повтор = TL заметно выше TR/BL/BR (контроль) и выше, чем на том же шаге при 1280.
usage: python3 ghost_edges.py <log> <capture> [<capture> ...]
"""
import os, re, subprocess, sys
import numpy as np
import cv2

FTP = 'ftp://192.168.1.199:5000/switch/shd-nx/logs/'
WORK = '/root/shd/caps/edges'
os.makedirs(WORK, exist_ok=True)
log = sys.argv[1]
lines = open(log, encoding='utf-8', errors='replace').read().split('\n')
caps = [int(x) for x in sys.argv[2:]]
listing = subprocess.run(['curl', '-s', '--max-time', '100', FTP], capture_output=True,
                         text=True).stdout
names = sorted(set(re.findall(r'(cap\d_r\d+_\S+\.bin)', listing)))
q = 4
for l in lines:
    m = re.search(r'internal render scale (\d)/4', l)
    if m:
        q = int(m.group(1))
        break
RX = re.compile(r'resolve color src fmt(\d+)@(\d+) key pitch (\d+) msaa (\d+) \(sample (\d)x(\d)\)'
                r'.*?rect (\d+),(\d+)-(\d+),(\d+) -> ([0-9A-F]{8}) tex fmt (\d+) exp (-?\d+)')


def section(n):
    s = None
    for i, l in enumerate(lines):
        if f'CAPTURE {n} armed' in l and s is None:
            s = i
        elif f'CAPTURE {n} done' in l and s is not None:
            return lines[s:i]
    return []


def pick(prefix, pitch):
    c = [x for x in names if x.startswith(prefix)]
    if len(c) <= 1:
        return c[0] if c else None
    want = min(pitch * q // 4, 1280) if q != 4 else min(pitch, 1280)
    for x in c:
        if int(re.search(r'_(\d+)x(\d+)_vk\d+\.bin$', x).group(1)) == want:
            return x
    return c[-1]


def load(name):
    path = os.path.join(WORK, name)
    if not os.path.exists(path):
        subprocess.run(['curl', '-s', '--max-time', '200', '-o', path, FTP + name])
    w, h, vk = map(int, re.search(r'_(\d+)x(\d+)_vk(\d+)\.bin$', name).groups())
    raw = open(path, 'rb').read()
    if vk == 97:
        a = np.frombuffer(raw[:w * h * 8], dtype=np.float16).astype(np.float32).reshape(h, w, 4)
        g = np.nan_to_num(a[:, :, :3].mean(axis=2))
        return np.log1p(np.clip(g, 0, 1e4))
    if vk in (37, 64):
        a = np.frombuffer(raw[:w * h * 4], dtype=np.uint8).reshape(h, w, 4)
        return a[:, :, :3].astype(np.float32).mean(axis=2) / 255.0
    return None


def edges(img):
    gx = cv2.Sobel(img, cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(img, cv2.CV_32F, 0, 1, ksize=3)
    return np.sqrt(gx * gx + gy * gy)


def corr(a, b):
    a = a.ravel() - a.mean()
    b = b.ravel() - b.mean()
    d = np.sqrt((a * a).sum() * (b * b).sum())
    return float((a * b).sum() / d) if d > 0 else 0.0


def scores(g):
    h, w = g.shape
    h2, w2 = h // 2, w // 2
    g = g[:h2 * 2, :w2 * 2]
    small = cv2.resize(g, (w2, h2), interpolation=cv2.INTER_AREA)
    es = edges(small)
    out = []
    for (y, x) in ((0, 0), (0, w2), (h2, 0), (h2, w2)):
        out.append(corr(edges(g[y:y + h2, x:x + w2]), es))
    return out


for n in caps:
    sec = section(n)
    print(f'\n=== {os.path.basename(log)} захват {n}, масштаб {q}/4 '
          f'(контуры: TL против контроля TR/BL/BR)')
    k = 0
    for l in sec:
        m = RX.search(l)
        if not m:
            continue
        k += 1
        fmt, b, pitch, msaa, sx, sy, x0, y0, x1, y1, to, tex, exp = m.groups()
        rw, rh = int(x1) - int(x0), int(y1) - int(y0)
        if rw < 300:
            continue
        prefix = f'cap{n}_r{k:02d}_fmt{fmt}_b{b}_rect{rw}x{rh}_to{to}_tex{tex}_exp{exp}_'
        f = pick(prefix, int(pitch))
        if not f:
            print(f'  r{k:02d}: файла нет')
            continue
        g = load(f)
        if g is None:
            continue
        img_w = int(re.search(r'_(\d+)x(\d+)_vk', f).group(1))
        sw = int(sx) if int(msaa) else 1
        sh = int(sy) if int(msaa) else 1
        scale = img_w / min(int(pitch), 1280) if int(pitch) else 1
        aw = min(g.shape[1], int(round(rw * sw * scale)))
        ah = min(g.shape[0], int(round(rh * sh * scale)))
        s = scores(g[:ah, :aw])
        flag = '  <== ПОВТОР' if s[0] > 0.15 and s[0] > 2.5 * max(abs(v) for v in s[1:]) else ''
        print(f'  r{k:02d} pitch {pitch:>4} msaa {msaa} {rw}x{rh} -> {to} exp {exp:>3} | '
              f'TL {s[0]:+.3f}  TR {s[1]:+.3f}  BL {s[2]:+.3f}  BR {s[3]:+.3f}{flag}')
