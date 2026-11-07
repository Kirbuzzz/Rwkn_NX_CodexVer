/* Herramienta de PC: apk_cli extract <apk> <carpeta>  |  apk_cli read <apk> <entrada> <salida> */
#include "../source/rdl/rdl_apk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool prog(rdl_phase p, uint64_t d, uint64_t t, void *u)
{ (void)p; (void)u; fprintf(stderr, "\r%llu/%llu KB", (unsigned long long)(d >> 10), (unsigned long long)(t >> 10)); return true; }

int main(int argc, char **argv)
{
    if (argc >= 4 && !strcmp(argv[1], "extract")) {
        bool did; rdl_result r = rdl_apk_extract(argv[2], argv[3], prog, NULL, &did);
        fprintf(stderr, "\n-> %s (did_work=%d)\n", rdl_strerror(r), did); return r != RDL_OK;
    }
    if (argc >= 5 && !strcmp(argv[1], "read")) {
        void *d; size_t n; rdl_result r = rdl_apk_read(argv[2], argv[3], &d, &n);
        if (r == RDL_OK) { FILE *f = fopen(argv[4], "wb"); fwrite(d, 1, n, f); fclose(f); free(d); }
        fprintf(stderr, "-> %s (%zu bytes)\n", rdl_strerror(r), n); return r != RDL_OK;
    }
    fprintf(stderr, "uso: apk_cli extract APK DIR | read APK ENTRADA SALIDA\n"); return 2;
}
