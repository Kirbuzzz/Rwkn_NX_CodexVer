#include "../source/rdl/rdl.h"
#include "../source/rdl/rdl_md5.h"
#include "../source/rdl/rdl_apk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern const char *fake_manifest_body, *fake_src_path;
extern long long fake_fail_after, fake_last_resume;
extern int fake_ignore_range, fake_fail_all, fake_perform_calls;

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("  FALLO linea %d: %s\n", __LINE__, #c); } } while (0)

#define DIR   "/tmp/rdl_test/obb"
#define NAME  "main.test.obb"
#define FINAL DIR "/" NAME
#define PART  FINAL ".part"
#define SRC   "/tmp/rdl_test/src.bin"
#define MIB   (1024ull * 1024)

static long long fsize(const char *p) { struct stat st; return stat(p, &st) ? -1 : (long long)st.st_size; }

static void md5_of(const char *path, char out[33])
{
    FILE *f = fopen(path, "rb"); rdl_md5_ctx c; rdl_md5_init(&c);
    char b[8192]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) rdl_md5_update(&c, b, n);
    fclose(f); uint8_t d[16]; rdl_md5_final(&c, d); rdl_md5_hex(d, out);
}

static void make_file(const char *path, size_t size, uint32_t seed)
{
    FILE *f = fopen(path, "wb"); uint32_t x = seed ? seed : 1;
    for (size_t i = 0; i < size; i++) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; fputc((int)(x & 0xff), f); }
    fclose(f);
}

static void reset(void)
{
    system("rm -rf " DIR " && mkdir -p " DIR);
    fake_fail_after = -1; fake_ignore_range = 0; fake_fail_all = 0; fake_perform_calls = 0; fake_last_resume = -1;
}

static rdl_obb_info mkinfo(const char *srcfile, size_t size)
{
    rdl_obb_info o; memset(&o, 0, sizeof o);
    o.valid = true; snprintf(o.file_name, sizeof o.file_name, "%s", NAME);
    snprintf(o.url, sizeof o.url, "https://fake.test/dl/x.obb");
    o.size = size; md5_of(srcfile, o.md5); return o;
}

static int cancel_after = 0, cb_calls = 0;
static bool cb_cancel(rdl_phase p, uint64_t d, uint64_t t, void *u) { (void)p; (void)d; (void)t; (void)u; return ++cb_calls < cancel_after; }
static uint64_t last_done = 0;
static bool cb_track(rdl_phase p, uint64_t d, uint64_t t, void *u) { (void)u; if (p == RDL_PHASE_DOWNLOAD) last_done = d; (void)t; return true; }


static uint64_t ext_done = 0, ext_total = 0; static int ext_ok = 1;
static bool cb_ext(rdl_phase p, uint64_t d, uint64_t t, void *u)
{ (void)u; if (p != RDL_PHASE_EXTRACT || d < ext_done) ext_ok = 0; ext_done = d; ext_total = t; return true; }

static char *slurp(const char *p) { FILE *f = fopen(p, "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1); fread(b, 1, (size_t)n, f); b[n] = 0; fclose(f); return b; }

int main(void)
{
    system("rm -rf /tmp/rdl_test && mkdir -p /tmp/rdl_test");
    char hex[33]; uint8_t d[16]; rdl_md5_ctx c;

    puts("[md5] vectores conocidos");
    rdl_md5_init(&c); rdl_md5_final(&c, d); rdl_md5_hex(d, hex); CHECK(!strcmp(hex, "d41d8cd98f00b204e9800998ecf8427e"));
    rdl_md5_init(&c); rdl_md5_update(&c, "abc", 3); rdl_md5_final(&c, d); rdl_md5_hex(d, hex); CHECK(!strcmp(hex, "900150983cd24fb0d6963f7d28e17f72"));
    rdl_md5_init(&c); for (int i = 0; i < 1000; i++) rdl_md5_update(&c, "aaaaaaaaaa", 10); /* fragmentado */
    rdl_md5_final(&c, d); rdl_md5_hex(d, hex);
    rdl_md5_init(&c); { char *m = malloc(10000); memset(m, 'a', 10000); rdl_md5_update(&c, m, 10000); free(m); }
    { uint8_t d2[16]; char h2[33]; rdl_md5_final(&c, d2); rdl_md5_hex(d2, h2); CHECK(!strcmp(hex, h2)); }
    rdl_md5_init(&c); { char *m = malloc(1000000); memset(m, 'a', 1000000); rdl_md5_update(&c, m, 1000000); free(m); }
    rdl_md5_final(&c, d); rdl_md5_hex(d, hex); CHECK(!strcmp(hex, "7707d6ae4e027c70eea2a935c2296f21"));

    puts("[manifest] real");
    char *real = slurp("tests/manifest_real.json"); rdl_manifest m;
    CHECK(rdl_manifest_parse(real, strlen(real), &m) == 0);
    CHECK(m.normal.valid && m.normal.size == 424073570ull && !strcmp(m.normal.md5, "85d9746c62701b5824ba89f819d35aa5"));
    CHECK(!strcmp(m.normal.file_name, "main.2086.com.ubisoft.raymanadventures.obb"));
    CHECK(m.hd.valid && m.hd.size == 506302828ull && !strcmp(m.hd.md5, "9a7abdebc034bae5043a01d1d01bdb7a"));
    CHECK(strstr(m.hd.url, "/obbhd-v1-") && strstr(m.normal.url, "/obb-v2-"));
    CHECK(m.apk.valid && m.apk.size == 66182220ull && !strcmp(m.apk.md5, "d95d5bea03ae14bf08835bfd47523c1c") && !strcmp(m.apk.file_name, "Reawaken.apk"));

    puts("[manifest] casos invalidos");
    const char *bad_traversal = "{\"reawaken\":{\"obb\":{\"fileName\":\"../x.obb\",\"url\":\"https://a/b\",\"size\":10,\"md5\":\"85d9746c62701b5824ba89f819d35aa5\"}}}";
    const char *bad_http = "{\"reawaken\":{\"obb\":{\"fileName\":\"a.obb\",\"url\":\"http://a/b\",\"size\":10,\"md5\":\"85d9746c62701b5824ba89f819d35aa5\"}}}";
    const char *bad_md5 = "{\"reawaken\":{\"obb\":{\"fileName\":\"a.obb\",\"url\":\"https://a/b\",\"size\":10,\"md5\":\"zz\"}}}";
    const char *bad_big = "{\"reawaken\":{\"obb\":{\"fileName\":\"a.obb\",\"url\":\"https://a/b\",\"size\":5000000000,\"md5\":\"85d9746c62701b5824ba89f819d35aa5\"}}}";
    const char *no_hd = "{\"reawaken\":{\"obb\":{\"fileName\":\"a.obb\",\"url\":\"https://a/b\",\"size\":10,\"md5\":\"85D9746C62701B5824BA89F819D35AA5\"}}}";
    CHECK(rdl_manifest_parse(bad_traversal, strlen(bad_traversal), &m) == -2);
    CHECK(rdl_manifest_parse(bad_http, strlen(bad_http), &m) == -2);
    CHECK(rdl_manifest_parse(bad_md5, strlen(bad_md5), &m) == -2);
    CHECK(rdl_manifest_parse(bad_big, strlen(bad_big), &m) == -2);
    CHECK(rdl_manifest_parse("{}", 2, &m) == -1);
    CHECK(rdl_manifest_parse("basura", 6, &m) == -1);
    { const char *trunc = "{\"reawaken\":{\"obb\":{\"fileName\":\"a"; CHECK(rdl_manifest_parse(trunc, strlen(trunc), &m) == -2); }   /* truncado */
    CHECK(rdl_manifest_parse(no_hd, strlen(no_hd), &m) == 0 && !m.hd.valid && !strcmp(m.normal.md5, "85d9746c62701b5824ba89f819d35aa5"));

    puts("[tls] cacert embebido");
    { system("rm -rf /tmp/rdl_test/ca && mkdir -p /tmp/rdl_test/ca");
      rdl_config cc = { "https://fake.test/", "/tmp/rdl_test/ca/sub/cacert.pem", NULL, false, "-----BEGIN-----\nPEM\n", 20 };
      CHECK(rdl_init(&cc) == RDL_OK);
      CHECK(fsize("/tmp/rdl_test/ca/sub/cacert.pem") == 20);
      { FILE *f = fopen("/tmp/rdl_test/ca/sub/cacert.pem", "wb"); fputs("MIO", f); fclose(f); }
      rdl_exit(); CHECK(rdl_init(&cc) == RDL_OK);
      CHECK(fsize("/tmp/rdl_test/ca/sub/cacert.pem") == 3);               /* no pisa el del usuario */
      rdl_exit();
      rdl_config real = { "https://fake.test/", NULL, NULL, false, rdl_cacert_pem, rdl_cacert_pem_len };
      CHECK(rdl_cacert_pem_len > 100000 && strncmp(rdl_cacert_pem, "-----BEGIN CERTIFICATE-----", 27) == 0);
      CHECK(rdl_init(&real) == RDL_OK); rdl_exit(); }
    rdl_config cfg = { "https://fake.test/", NULL, NULL, false };
    CHECK(rdl_init(&cfg) == RDL_OK);
    { rdl_config bad = { "http://x", NULL, NULL, false }; CHECK(rdl_init(&bad) == RDL_ERR_ARG); }

    make_file(SRC, 5 * MIB, 12345);
    fake_src_path = SRC;
    rdl_obb_info info = mkinfo(SRC, 5 * MIB);

    puts("[descarga] completa");
    reset();
    CHECK(rdl_obb_download(&info, DIR, cb_track, NULL) == RDL_OK);
    CHECK(fsize(FINAL) == (long long)(5 * MIB) && fsize(PART) == -1);
    CHECK(last_done == 5 * MIB);
    md5_of(FINAL, hex); CHECK(!strcmp(hex, info.md5));

    puts("[descarga] ya instalada: no toca la red");
    fake_perform_calls = 0;
    CHECK(rdl_obb_download(&info, DIR, NULL, NULL) == RDL_OK);
    CHECK(fake_perform_calls == 0);
    CHECK(rdl_obb_verify(&info, DIR, NULL, NULL) == RDL_OK);

    puts("[descarga] corte y reanudacion");
    reset(); fake_fail_after = (long long)(2 * MIB);
    CHECK(rdl_obb_download(&info, DIR, NULL, NULL) == RDL_ERR_NET);
    long long partial = fsize(PART);
    CHECK(partial >= (long long)(2 * MIB) && partial < (long long)(5 * MIB) && fsize(FINAL) == -1);
    fake_fail_after = -1; last_done = 0;
    CHECK(rdl_obb_download(&info, DIR, cb_track, NULL) == RDL_OK);
    CHECK(fake_last_resume == partial);              /* pidio Range desde donde quedo */
    CHECK(last_done == 5 * MIB);                      /* el progreso incluye lo ya descargado */
    md5_of(FINAL, hex); CHECK(!strcmp(hex, info.md5) && fsize(PART) == -1);

    puts("[descarga] servidor sin soporte de Range");
    reset();
    { char cmd[256]; snprintf(cmd, sizeof cmd, "head -c %llu " SRC " > " PART, (unsigned long long)MIB); system(cmd); }
    fake_ignore_range = 1; fake_perform_calls = 0;
    CHECK(rdl_obb_download(&info, DIR, NULL, NULL) == RDL_OK);
    CHECK(fake_perform_calls == 2 && fake_last_resume == 0);
    md5_of(FINAL, hex); CHECK(!strcmp(hex, info.md5));

    puts("[descarga] MD5 incorrecto");
    reset();
    { rdl_obb_info bad = info; memset(bad.md5, '0', 32); bad.md5[32] = 0;
      CHECK(rdl_obb_download(&bad, DIR, NULL, NULL) == RDL_ERR_MD5);
      CHECK(fsize(PART) == -1 && fsize(FINAL) == -1); }

    puts("[descarga] cancelacion conserva el .part");
    reset(); cancel_after = 3; cb_calls = 0;
    CHECK(rdl_obb_download(&info, DIR, cb_cancel, NULL) == RDL_ERR_CANCELLED);
    CHECK(fsize(PART) > 0 && fsize(FINAL) == -1);
    CHECK(rdl_obb_download(&info, DIR, NULL, NULL) == RDL_OK);   /* y luego reanuda */

    puts("[descarga] reemplazo de otra variante (normal -> HD)");
    reset();
    make_file("/tmp/rdl_test/hd.bin", 6 * MIB, 999);
    rdl_obb_info hd = mkinfo("/tmp/rdl_test/hd.bin", 6 * MIB);
    CHECK(rdl_obb_download(&info, DIR, NULL, NULL) == RDL_OK);
    fake_src_path = "/tmp/rdl_test/hd.bin";
    fake_fail_after = (long long)(3 * MIB);
    CHECK(rdl_obb_download(&hd, DIR, NULL, NULL) == RDL_ERR_NET);
    CHECK(fsize(FINAL) == (long long)(5 * MIB));                /* el viejo sigue intacto hasta validar el nuevo */
    fake_fail_after = -1;
    CHECK(rdl_obb_download(&hd, DIR, NULL, NULL) == RDL_OK);
    CHECK(fsize(FINAL) == (long long)(6 * MIB) && rdl_obb_verify(&hd, DIR, NULL, NULL) == RDL_OK);
    fake_src_path = SRC;

    puts("[estado] rdl_obb_installed");
    rdl_manifest mm; memset(&mm, 0, sizeof mm); mm.normal = info; mm.hd = hd;
    CHECK(rdl_obb_installed(&mm, DIR) == RDL_OBB_HD);
    reset(); CHECK(rdl_obb_installed(&mm, DIR) == RDL_OBB_NONE);
    make_file(FINAL, 5 * MIB, 1); CHECK(rdl_obb_installed(&mm, DIR) == RDL_OBB_NORMAL);
    CHECK(rdl_obb_verify(&info, DIR, NULL, NULL) == RDL_ERR_MD5);     /* mismo tamano, otro contenido */
    make_file(FINAL, 1234, 1);        CHECK(rdl_obb_installed(&mm, DIR) == RDL_OBB_OTHER);

    puts("[manifest] red y cache");
    reset(); fake_manifest_body = real;
    CHECK(rdl_fetch_manifest(&mm, "/tmp/rdl_test/data/manifest.cache.json") == RDL_OK && !mm.from_cache);
    CHECK(mm.normal.size == 424073570ull && fsize("/tmp/rdl_test/data/manifest.cache.json") > 100);
    fake_fail_all = 1;
    CHECK(rdl_fetch_manifest(&mm, "/tmp/rdl_test/data/manifest.cache.json") == RDL_OK && mm.from_cache && mm.hd.valid);
    CHECK(rdl_fetch_manifest(&mm, "/tmp/rdl_test/no_existe.json") == RDL_ERR_NET);
    CHECK(strstr(rdl_last_error(), "curl 7") && strstr(rdl_last_error(), "Couldn't connect"));
    fake_fail_all = 0; fake_manifest_body = "{\"x\":1}";
    CHECK(rdl_fetch_manifest(&mm, NULL) == RDL_ERR_MANIFEST);
    CHECK(strstr(rdl_last_error(), "no es el manifest"));


    puts("[apk] extraccion");
    {
        const char *APK = "/tmp/rdl_test/test.apk", *APP = "/tmp/rdl_test/app";
        bool did = false;
        system("rm -rf /tmp/rdl_test/app* /tmp/rdl_test/zsrc* /tmp/rdl_test/*.apk /tmp/rdl_test/evil.txt");
        CHECK(system("python3 tests/make_zip.py /tmp/rdl_test/test.apk /tmp/rdl_test/zsrc 1") == 0);

        ext_done = 0; ext_ok = 1;
        CHECK(rdl_apk_extract(APK, APP, cb_ext, NULL, &did) == RDL_OK && did);
        CHECK(ext_ok && ext_done == ext_total && ext_total == 3 * MIB + 7000 + 100000);
        CHECK(system("cmp -s /tmp/rdl_test/zsrc/libuaf.so /tmp/rdl_test/app/lib/libuaf.so") == 0);
        CHECK(system("cmp -s /tmp/rdl_test/zsrc/a.txt /tmp/rdl_test/app/assets/a.txt") == 0);
        CHECK(system("cmp -s /tmp/rdl_test/zsrc/sub/b.bin /tmp/rdl_test/app/assets/sub/b.bin") == 0);
        CHECK(system("test ! -e /tmp/rdl_test/app/evil.txt && test ! -e /tmp/rdl_test/evil.txt "
                     "&& test ! -e /tmp/rdl_test/app/assets/dobleslash.txt && test ! -e /tmp/rdl_test/app/lib/libother.so") == 0);
        CHECK(system("test -z \"$(find /tmp/rdl_test/app -name '*.tmp')\" && test -f /tmp/rdl_test/app/lib/.apk_stamp") == 0);

        did = true;
        CHECK(rdl_apk_extract(APK, APP, NULL, NULL, &did) == RDL_OK && !did);        /* mismo apk: no repite */

        CHECK(system("python3 tests/make_zip.py /tmp/rdl_test/test2.apk /tmp/rdl_test/zsrc2 2") == 0);
        CHECK(rdl_apk_extract("/tmp/rdl_test/test2.apk", APP, NULL, NULL, &did) == RDL_OK && did);  /* apk nuevo */
        CHECK(system("cmp -s /tmp/rdl_test/zsrc2/libuaf.so /tmp/rdl_test/app/lib/libuaf.so") == 0);

        puts("[apk] errores");
        CHECK(system("python3 tests/make_zip.py /tmp/rdl_test/corrupt.apk /tmp/rdl_test/zsrc3 3") == 0);
        { FILE *f = fopen("/tmp/rdl_test/corrupt.apk", "r+b"); fseek(f, 5000, SEEK_SET); int b = fgetc(f);
          fseek(f, 5000, SEEK_SET); fputc(b ^ 0xFF, f); fclose(f); }
        CHECK(rdl_apk_extract("/tmp/rdl_test/corrupt.apk", "/tmp/rdl_test/app2", NULL, NULL, &did) == RDL_ERR_APK);
        CHECK(system("test ! -e /tmp/rdl_test/app2/lib/libuaf.so && test ! -e /tmp/rdl_test/app2/lib/.apk_stamp "
                     "&& test -z \"$(find /tmp/rdl_test/app2 -name '*.tmp' 2>/dev/null)\"") == 0);

        cancel_after = 3; cb_calls = 0;
        CHECK(rdl_apk_extract(APK, "/tmp/rdl_test/app3", cb_cancel, NULL, &did) == RDL_ERR_CANCELLED);
        CHECK(system("test ! -e /tmp/rdl_test/app3/lib/libuaf.so && test ! -e /tmp/rdl_test/app3/lib/.apk_stamp "
                     "&& test -z \"$(find /tmp/rdl_test/app3 -name '*.tmp' 2>/dev/null)\"") == 0);
        CHECK(rdl_apk_extract(APK, "/tmp/rdl_test/app3", NULL, NULL, &did) == RDL_OK && did);   /* y luego completa */

        CHECK(rdl_apk_extract("/tmp/rdl_test/no_existe.apk", "/tmp/rdl_test/app4", NULL, NULL, &did) == RDL_ERR_IO);
        { FILE *f = fopen("/tmp/rdl_test/basura.apk", "wb"); for (int i = 0; i < 500; i++) fputs("hola", f); fclose(f); }
        CHECK(rdl_apk_extract("/tmp/rdl_test/basura.apk", "/tmp/rdl_test/app4", NULL, NULL, &did) == RDL_ERR_APK);
        CHECK(system("python3 tests/make_zip.py /tmp/rdl_test/nolib.apk /tmp/rdl_test/zsrc4 4 nolib") == 0);
        CHECK(rdl_apk_extract("/tmp/rdl_test/nolib.apk", "/tmp/rdl_test/app4", NULL, NULL, &did) == RDL_ERR_APK);
        CHECK(rdl_apk_extract(NULL, APP, NULL, NULL, &did) == RDL_ERR_ARG);

        puts("[apk] lectura de una entrada");
        void *data = NULL; size_t len = 0;
        CHECK(rdl_apk_read(APK, "res/drawable/logorc.png", &data, &len) == RDL_OK && len == 50000);
        { char *orig = slurp("/tmp/rdl_test/zsrc/logorc.png"); CHECK(data && memcmp(orig, data, 50000) == 0); free(orig); }
        free(data);
        CHECK(rdl_apk_read(APK, "res/no_existe.png", &data, &len) == RDL_ERR_APK && data == NULL);
    }

    rdl_exit(); free(real);
    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
