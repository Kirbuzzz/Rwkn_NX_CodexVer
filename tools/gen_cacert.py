#!/usr/bin/env python3
"""Escribe source/rdl/rdl_cacert.c con el bundle de CA de Mozilla (paquete certifi).
Se usa si no hay cacert.pem en la SD. Uso: python3 tools/gen_cacert.py [ruta/cacert.pem]"""
import sys, certifi
src = sys.argv[1] if len(sys.argv) > 1 else certifi.where()
blocks, cur = [], None
for line in open(src, encoding="ascii", errors="ignore"):
    line = line.strip()
    if line == "-----BEGIN CERTIFICATE-----": cur = [line]
    elif line == "-----END CERTIFICATE-----" and cur is not None:
        cur.append(line); blocks.append(cur); cur = None
    elif cur is not None: cur.append(line)
with open("source/rdl/rdl_cacert.c", "w") as f:
    f.write("/* GENERADO por tools/gen_cacert.py: bundle de CA de Mozilla (MPL-2.0) via certifi %s.\n" % getattr(certifi, "__version__", "?"))
    f.write("   Puede quedar desactualizado: una copia mas nueva en <carpeta>/cacert.pem tiene prioridad. */\n")
    f.write("#include \"rdl.h\"\n\nconst char rdl_cacert_pem[] =\n")
    for b in blocks:
        for l in b: f.write('  "%s\\n"\n' % l)
    f.write("  ;\nconst unsigned int rdl_cacert_pem_len = sizeof rdl_cacert_pem - 1;\n")
print(len(blocks), "certificados")
