#!/usr/bin/env python3
"""Genera PNG de prueba (todos los tipos de color/profundidad, filtros 0-4 forzados) + su RGBA esperado.
uso: make_pngs.py OUTDIR"""
import os, random, struct, sys, zlib

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
rnd = random.Random(7)

def chunk(t, d):
    c = struct.pack(">I", len(d)) + t + d
    return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)

def paeth(a, b, c):
    p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else (b if pb <= pc else c)

def filt(ft, cur, prev, bpp):
    o = bytearray()
    for i, x in enumerate(cur):
        a = cur[i - bpp] if i >= bpp else 0
        b = prev[i] if prev else 0
        c = prev[i - bpp] if (prev and i >= bpp) else 0
        pr = [0, a, b, (a + b) // 2, paeth(a, b, c)][ft]
        o.append((x - pr) & 255)
    return bytes(o)

def write_png(name, w, h, depth, ctype, rows, plte=None, trns=None):
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    bpp = max(1, (ch * depth + 7) // 8)
    raw = bytearray(); prev = None
    for y, row in enumerate(rows):
        ft = y % 5
        raw.append(ft); raw += filt(ft, row, prev, bpp); prev = row
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, depth, ctype, 0, 0, 0))
    if plte: data += chunk(b"PLTE", plte)
    if trns: data += chunk(b"tRNS", trns)
    half = max(1, len(raw) // 2)                       # IDAT partido en dos
    z = zlib.compress(bytes(raw), 6)
    data += chunk(b"IDAT", z[: len(z) // 2]) + chunk(b"IDAT", z[len(z) // 2:]) + chunk(b"IEND", b"")
    open(os.path.join(out, name + ".png"), "wb").write(data)

def pack_bits(vals, depth):
    o = bytearray(); cur = 0; n = 0
    for v in vals:
        cur = (cur << depth) | v; n += depth
        if n == 8: o.append(cur); cur = 0; n = 0
    if n: o.append(cur << (8 - n))
    return bytes(o)

def emit(name, w, h, rgba): open(os.path.join(out, name + ".rgba"), "wb").write(bytes(rgba))

for w, h in [(1, 1), (7, 5), (33, 17)]:
    # gris 1/2/4/8/16
    for depth in (1, 2, 4, 8, 16):
        rows, exp = [], bytearray()
        for y in range(h):
            vals = [rnd.randrange(1 << depth) for _ in range(w)]
            if depth == 16: rows.append(b"".join(struct.pack(">H", v) for v in vals))
            else: rows.append(pack_bits(vals, depth))
            for v in vals:
                g = (v >> 8) if depth == 16 else (v * 255 // ((1 << depth) - 1) if depth < 8 else v)
                exp += bytes([g, g, g, 255])
        write_png(f"gray{depth}_{w}x{h}", w, h, depth, 0, rows); emit(f"gray{depth}_{w}x{h}", w, h, exp)
    # RGB / RGBA / gris+alfa 8 y 16
    for ctype, ch in ((2, 3), (6, 4), (4, 2)):
        for depth in (8, 16):
            rows, exp = [], bytearray()
            for y in range(h):
                px = [[rnd.randrange(1 << depth) for _ in range(ch)] for _ in range(w)]
                rows.append(b"".join(struct.pack(">H" if depth == 16 else "B", s) for p in px for s in p))
                for p in px:
                    s = [(v >> 8) if depth == 16 else v for v in p]
                    if ctype == 2: exp += bytes([s[0], s[1], s[2], 255])
                    elif ctype == 6: exp += bytes(s)
                    else: exp += bytes([s[0], s[0], s[0], s[1]])
            n = f"c{ctype}d{depth}_{w}x{h}"; write_png(n, w, h, depth, ctype, rows); emit(n, w, h, exp)
    # paleta 1/2/4/8 con y sin tRNS
    for depth in (1, 2, 4, 8):
        n_col = min(1 << depth, 200)
        plte = bytes(rnd.randrange(256) for _ in range(n_col * 3))
        alpha = bytes(rnd.randrange(256) for _ in range(n_col))
        for use_t in (False, True):
            rows, exp = [], bytearray()
            for y in range(h):
                idx = [rnd.randrange(n_col) for _ in range(w)]
                rows.append(pack_bits(idx, depth) if depth < 8 else bytes(idx))
                for i in idx: exp += plte[i*3:i*3+3] + bytes([alpha[i] if use_t else 255])
            n = f"pal{depth}{'t' if use_t else ''}_{w}x{h}"
            write_png(n, w, h, depth, 3, rows, plte, alpha if use_t else None); emit(n, w, h, exp)
print(len(os.listdir(out)) // 2, "png generados")
