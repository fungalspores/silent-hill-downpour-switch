"""Ищет Texture2D с DXT5 в UE3-пакете SHD и печатает имена/форматы.
python3 find_tex.py <pkg.xxx> [фильтр-имени]"""
import sys

sys.path.insert(0, "/root/shd/tools")
import ue3pkg  # noqa: E402

path = sys.argv[1]
flt = (sys.argv[2] if len(sys.argv) > 2 else "").lower()
pkg = ue3pkg.Package(path)
print("экспортов:", len(pkg.exports), "имён:", len(pkg.names))
seen = {}
for e in pkg.exports:
    cls = pkg.class_name(e)
    if "Texture" not in cls:
        continue
    name = pkg.export_name(e)
    if flt and flt not in name.lower():
        continue
    seen.setdefault(cls, []).append((name, e))
for cls, items in seen.items():
    print(f"== {cls}: {len(items)}")
    for name, _ in items[:40]:
        print("   ", name)
