/* MD5 minimo y autocontenido (RFC 1321). Solo para verificar integridad de descargas. */
#ifndef RDL_MD5_H
#define RDL_MD5_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t s[4];
    uint64_t len;       /* bytes procesados */
    uint8_t  buf[64];
    size_t   buflen;
} rdl_md5_ctx;

void rdl_md5_init(rdl_md5_ctx *c);
void rdl_md5_update(rdl_md5_ctx *c, const void *data, size_t n);
void rdl_md5_final(rdl_md5_ctx *c, uint8_t out[16]);
void rdl_md5_hex(const uint8_t d[16], char out[33]);   /* hex en minusculas */

#endif
