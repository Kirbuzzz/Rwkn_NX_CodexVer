#!/usr/bin/env python3
"""Rasteriza DejaVu Sans Bold (Latin-1) a un atlas de cobertura 8 bits, comprimido con zlib,
y lo escribe como source/rdl/rdl_font_data.c. Uso: python3 tools/gen_font.py [ruta.ttf]"""
import struct, sys, zlib
from PIL import Image, ImageDraw, ImageFont

TTF = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
SIZES = [22, 30, 44]
FIRST, LAST = 32, 255

hdr = b"RFNT" + bytes([len(SIZES)])
tabs, pix = [], bytearray()
size_hdrs = b""
for px in SIZES:
    font = ImageFont.truetype(TTF, px)
    asc, desc = font.getmetrics()
    size_hdrs += struct.pack("<BbBB", px, asc, asc + desc, 0)
    tab = b""
    for cp in range(FIRST, LAST + 1):
        ch = chr(cp)
        adv = int(round(font.getlength(ch)))
        l, t, r, b = font.getbbox(ch, anchor="ls")
        w, h = max(0, r - l), max(0, b - t)
        if cp == 0xA0 or ch.isspace() or w == 0 or h == 0:
            w = h = 0
        off = len(pix)
        if w and h:
            im = Image.new("L", (w, h), 0)
            ImageDraw.Draw(im).text((-l, -t), ch, font=font, fill=255, anchor="ls")
            pix += im.tobytes()
        assert 0 <= adv <= 255 and w <= 255 and h <= 255 and -128 <= l <= 127 and -128 <= -t <= 127, (cp, adv, w, h, l, t)
        tab += struct.pack("<BbbBBI", adv, l, -t, w, h, off)
    tabs.append(tab)

raw = hdr + size_hdrs + b"".join(tabs) + bytes(pix)
comp = zlib.compress(raw, 9)
with open("source/rdl/rdl_font_data.c", "w") as f:
    f.write("/* GENERADO por tools/gen_font.py (DejaVu Sans Bold, licencia Bitstream Vera/DejaVu). No editar. */\n")
    f.write("#include \"rdl_gfx.h\"\n")
    f.write("const unsigned int rdl_font_raw_size = %d;\n" % len(raw))
    f.write("const unsigned int rdl_font_blob_size = %d;\n" % len(comp))
    f.write("const unsigned char rdl_font_blob[] = {\n")
    for i in range(0, len(comp), 24):
        f.write("  " + ",".join("0x%02x" % c for c in comp[i:i+24]) + ",\n")
    f.write("};\n")
print("raw", len(raw), "comprimido", len(comp), "tamanos", SIZES)
