/* Cabecera minima que imita la API de libcurl usada por rdl.c. SOLO para pruebas en PC. */
#ifndef FAKE_CURL_H
#define FAKE_CURL_H
#include <stddef.h>
typedef void CURL;
struct curl_blob { void *data; size_t len; unsigned int flags; };
#define CURL_BLOB_COPY 1
typedef long long curl_off_t;
#define LIBCURL_VERSION_NUM 0x080000
#define CURL_GLOBAL_DEFAULT 3L
#define CURL_ERROR_SIZE 256
#define CURLPROTO_HTTPS 2
typedef enum {
    CURLE_OK = 0, CURLE_COULDNT_CONNECT = 7, CURLE_PARTIAL_FILE = 18,
    CURLE_HTTP_RETURNED_ERROR = 22, CURLE_WRITE_ERROR = 23, CURLE_OPERATION_TIMEDOUT = 28,
    CURLE_RANGE_ERROR = 33, CURLE_ABORTED_BY_CALLBACK = 42
} CURLcode;
typedef enum {
    CURLOPT_URL = 1, CURLOPT_USERAGENT, CURLOPT_FOLLOWLOCATION, CURLOPT_MAXREDIRS, CURLOPT_CONNECTTIMEOUT,
    CURLOPT_LOW_SPEED_LIMIT, CURLOPT_LOW_SPEED_TIME, CURLOPT_NOSIGNAL, CURLOPT_FAILONERROR,
    CURLOPT_PROTOCOLS_STR, CURLOPT_REDIR_PROTOCOLS_STR, CURLOPT_PROTOCOLS, CURLOPT_REDIR_PROTOCOLS,
    CURLOPT_CAINFO, CURLOPT_SSL_VERIFYPEER, CURLOPT_SSL_VERIFYHOST, CURLOPT_WRITEFUNCTION, CURLOPT_WRITEDATA,
    CURLOPT_TIMEOUT, CURLOPT_XFERINFOFUNCTION, CURLOPT_XFERINFODATA, CURLOPT_NOPROGRESS, CURLOPT_BUFFERSIZE,
    CURLOPT_RESUME_FROM_LARGE, CURLOPT_ERRORBUFFER, CURLOPT_CAINFO_BLOB
} CURLoption;
typedef enum { CURLINFO_RESPONSE_CODE = 1 } CURLINFO;
CURLcode curl_global_init(long flags);
void curl_global_cleanup(void);
CURL *curl_easy_init(void);
CURLcode curl_easy_setopt(CURL *h, CURLoption opt, ...);
CURLcode curl_easy_perform(CURL *h);
CURLcode curl_easy_getinfo(CURL *h, CURLINFO info, ...);
void curl_easy_cleanup(CURL *h);
const char *curl_easy_strerror(CURLcode c);
#endif
