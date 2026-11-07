/* Implementacion falsa de libcurl: sirve archivos locales con soporte de Range y fallos simulados. */
#include <curl/curl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *fake_manifest_body = NULL;
const char *fake_src_path = NULL;
const char *fake_src_path2 = NULL;   /* si la URL contiene "/hd/" */
long        fake_chunk_delay_us = 0;
long long   fake_fail_after = -1;     /* corta con TIMEDOUT tras enviar >= N bytes */
int         fake_ignore_range = 0;    /* si resume>0 -> CURLE_RANGE_ERROR */
int         fake_fail_all = 0;        /* COULDNT_CONNECT siempre */
int         fake_perform_calls = 0;
long long   fake_last_resume = -1;

typedef struct {
    char url[1024];
    size_t (*wcb)(char *, size_t, size_t, void *); void *wdata;
    int (*xcb)(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t); void *xdata;
    curl_off_t resume;
} H;

const char *curl_easy_strerror(CURLcode c) { return c == CURLE_COULDNT_CONNECT ? "Couldn't connect to server" : "error simulado"; }
CURLcode curl_global_init(long f) { (void)f; return CURLE_OK; }
void curl_global_cleanup(void) {}
CURL *curl_easy_init(void) { return calloc(1, sizeof(H)); }
void curl_easy_cleanup(CURL *h) { free(h); }
CURLcode curl_easy_getinfo(CURL *h, CURLINFO i, ...) { (void)h; (void)i; return CURLE_OK; }

CURLcode curl_easy_setopt(CURL *hh, CURLoption opt, ...)
{
    H *h = hh; va_list ap; va_start(ap, opt);
    switch (opt) {
        case CURLOPT_URL: snprintf(h->url, sizeof h->url, "%s", va_arg(ap, const char *)); break;
        case CURLOPT_WRITEFUNCTION: h->wcb = va_arg(ap, size_t (*)(char *, size_t, size_t, void *)); break;
        case CURLOPT_WRITEDATA: h->wdata = va_arg(ap, void *); break;
        case CURLOPT_XFERINFOFUNCTION: h->xcb = va_arg(ap, int (*)(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)); break;
        case CURLOPT_XFERINFODATA: h->xdata = va_arg(ap, void *); break;
        case CURLOPT_RESUME_FROM_LARGE: h->resume = va_arg(ap, curl_off_t); break;
        default: break;
    }
    va_end(ap);
    return CURLE_OK;
}

CURLcode curl_easy_perform(CURL *hh)
{
    H *h = hh;
    fake_perform_calls++;
    if (fake_fail_all) return CURLE_COULDNT_CONNECT;

    if (strstr(h->url, "manifest.json")) {
        const char *b = fake_manifest_body ? fake_manifest_body : "";
        size_t n = strlen(b);
        return h->wcb((char *)b, 1, n, h->wdata) == n ? CURLE_OK : CURLE_WRITE_ERROR;
    }

    fake_last_resume = h->resume;
    if (fake_ignore_range && h->resume > 0) return CURLE_RANGE_ERROR;

    FILE *f = fopen((strstr(h->url, "/hd/") && fake_src_path2) ? fake_src_path2 : fake_src_path, "rb");
    if (!f) return CURLE_COULDNT_CONNECT;
    fseek(f, 0, SEEK_END); long long total = ftell(f);
    fseek(f, (long)h->resume, SEEK_SET);
    long long sent = 0, want = total - h->resume;
    char buf[65536]; size_t n; CURLcode rc = CURLE_OK;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        if (h->wcb(buf, 1, n, h->wdata) != n) { rc = CURLE_WRITE_ERROR; break; }
        sent += (long long)n;
        if (fake_chunk_delay_us) usleep((useconds_t)fake_chunk_delay_us);
        if (h->xcb && h->xcb(h->xdata, want, sent, 0, 0) != 0) { rc = CURLE_ABORTED_BY_CALLBACK; break; }
        if (fake_fail_after >= 0 && sent >= fake_fail_after && sent < want) { rc = CURLE_OPERATION_TIMEDOUT; break; }
    }
    fclose(f);
    return rc;
}
