#!/bin/sh
# Motor grafico: PNG, texto, pantallas. Uso: sh tests/run_gfx.sh [ruta/logorc.png]   (salida en /tmp/gfx)
set -e
mkdir -p /tmp/gfx/pngs && rm -f /tmp/gfx/pngs/* /tmp/gfx/*.rgba
python3 tests/make_pngs.py /tmp/gfx/pngs
gcc -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined \
    source/rdl/rdl_gfx.c source/rdl/rdl_png.c source/rdl/rdl_screen.c source/rdl/rdl_font_data.c tests/gfx_test.c -lz -lm -o /tmp/rnx_gfx_test
/tmp/rnx_gfx_test /tmp/gfx/pngs "$1" /tmp/gfx
