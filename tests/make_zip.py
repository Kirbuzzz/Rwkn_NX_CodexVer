#!/usr/bin/env python3
"""Genera un apk de prueba y deja los originales en SRCDIR para comparar.
uso: make_zip.py OUT SRCDIR SEED [nolib]"""
import os, random, sys, zipfile

out, src, seed = sys.argv[1], sys.argv[2], int(sys.argv[3])
nolib = len(sys.argv) > 4 and sys.argv[4] == "nolib"
r = random.Random(seed)
os.makedirs(os.path.join(src, "sub"), exist_ok=True)

libuaf = r.randbytes(1 << 20) + b"\0" * (1 << 20) + r.randbytes(1 << 20)
files = {
    "libuaf.so": libuaf,
    "a.txt": ("hola reawaken\n" * 500).encode(),
    "sub/b.bin": r.randbytes(100_000),
    "logorc.png": r.randbytes(50_000),
}
for k, v in files.items():
    with open(os.path.join(src, k), "wb") as f:
        f.write(v)

with zipfile.ZipFile(out, "w") as z:
    if not nolib:
        z.writestr(zipfile.ZipInfo("lib/arm64-v8a/libuaf.so"), libuaf, zipfile.ZIP_DEFLATED)
    z.writestr(zipfile.ZipInfo("lib/arm64-v8a/libother.so"), r.randbytes(10_000), zipfile.ZIP_STORED)
    z.writestr(zipfile.ZipInfo("assets/a.txt"), files["a.txt"], zipfile.ZIP_DEFLATED)
    z.writestr(zipfile.ZipInfo("assets/sub/b.bin"), files["sub/b.bin"], zipfile.ZIP_STORED)
    z.writestr(zipfile.ZipInfo("assets/dir/"), b"")
    z.writestr(zipfile.ZipInfo("assets/../evil.txt"), b"malo", zipfile.ZIP_STORED)
    z.writestr(zipfile.ZipInfo("/abs/evil.txt"), b"malo", zipfile.ZIP_STORED)
    z.writestr(zipfile.ZipInfo("assets//dobleslash.txt"), b"malo", zipfile.ZIP_STORED)
    z.writestr(zipfile.ZipInfo("res/drawable/logorc.png"), files["logorc.png"], zipfile.ZIP_DEFLATED)
