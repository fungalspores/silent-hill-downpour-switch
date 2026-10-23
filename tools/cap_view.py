"""Показать образ из захвата (cap*_r*.bin) картинкой.

Имя файла даёт всё нужное: ..._rect<W>x<H>_..._<IW>x<IH>_vk<F>.bin
  rect = прямоугольник resolve, IWxIH = размер образа, vk = VkFormat.
"""
import re, sys, struct
import numpy as np
import cv2

VK = {37: 'R8G8B8A8_UNORM', 64: 'A2B10G10R10', 97: 'R16G16B16A16_SFLOAT', 100: 'R32_SFLOAT'}

def load(path):
    m = re.search(r'rect(\d+)x(\d+).*?_(\d+)x(\d+)_vk(\d+)\.bin$', path)
    rw, rh, iw, ih, fmt = (int(x) for x in m.groups())
    raw = open(path, 'rb').read()
    print(f"{path.split('/')[-1]}\n  rect {rw}x{rh}, image {iw}x{ih}, vk{fmt} {VK.get(fmt,'?')}, {len(raw)} bytes")
    head = 0
    # данные могут идти после небольшого заголовка: подбираем по размеру
    for bpp in (8, 4, 2):
        need = iw * ih * bpp
        if len(raw) >= need:
            head = len(raw) - need
            break
    print(f"  предполагаю {bpp} байт/пиксель, заголовок {head}")
    body = raw[head:head + iw * ih * bpp]
    if fmt == 97:  # RGBA16F
        a = np.frombuffer(body, dtype=np.float16).astype(np.float32).reshape(ih, iw, 4)
        img = np.clip(a[:, :, :3] * 255.0, 0, 255).astype(np.uint8)
        img = img[:, :, ::-1]
    elif fmt == 37:
        a = np.frombuffer(body, dtype=np.uint8).reshape(ih, iw, 4)
        img = a[:, :, :3][:, :, ::-1]
    else:
        a = np.frombuffer(body, dtype=np.uint8).reshape(ih, -1)
        img = cv2.cvtColor(a[:, :iw], cv2.COLOR_GRAY2BGR)
    return img, rw, rh

if __name__ == '__main__':
    img, rw, rh = load(sys.argv[1])
    out = sys.argv[2]
    # отметим границу прямоугольника resolve
    vis = img.copy()
    cv2.rectangle(vis, (0, 0), (rw - 1, rh - 1), (0, 255, 0), 2)
    cv2.imwrite(out, vis)
    print("  ->", out)
