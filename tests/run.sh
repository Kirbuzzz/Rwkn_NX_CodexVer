#!/bin/sh
# Pruebas en PC del descargador (curl falso) + OpenSL ES. Desde la raiz del repo: sh tests/run.sh
set -e
gcc -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined -Itests -Isource/rdl \
    source/rdl/rdl.c source/rdl/rdl_cacert.c source/rdl/rdl_apk.c source/rdl/rdl_fs.c source/rdl/rdl_md5.c source/rdl/rdl_manifest.c \
    tests/fake_curl.c tests/host_test.c -lz -o /tmp/rnx_host_test
/tmp/rnx_host_test
gcc -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined -Isource tests/opensl_test.c source/opensl.c -lm -lpthread -o /tmp/rnx_opensl_test
ASAN_OPTIONS=detect_leaks=0 /tmp/rnx_opensl_test
gcc -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined -Isource \
    tests/modmenu_compat_test.c source/modmenu_compat.c -o /tmp/rnx_modmenu_compat_test
ASAN_OPTIONS=detect_leaks=0 /tmp/rnx_modmenu_compat_test
