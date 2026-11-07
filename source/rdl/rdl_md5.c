#include "rdl_md5.h"
#include <string.h>

static const uint32_t K[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
    0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
    0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
    0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
    0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
    0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
    0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
    0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
    0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
    0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
    0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
};
static const uint8_t R[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5, 9,14,20,5, 9,14,20,5, 9,14,20,5, 9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};
#define ROTL(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

static void transform(rdl_md5_ctx *c, const uint8_t blk[64])
{
    uint32_t M[16];
    for (int i = 0; i < 16; i++)
        M[i] = (uint32_t)blk[i*4] | ((uint32_t)blk[i*4+1] << 8) |
               ((uint32_t)blk[i*4+2] << 16) | ((uint32_t)blk[i*4+3] << 24);

    uint32_t A = c->s[0], B = c->s[1], C = c->s[2], D = c->s[3];
    for (int i = 0; i < 64; i++) {
        uint32_t F; int g;
        if (i < 16)      { F = (B & C) | (~B & D); g = i; }
        else if (i < 32) { F = (D & B) | (~D & C); g = (5*i + 1) & 15; }
        else if (i < 48) { F = B ^ C ^ D;          g = (3*i + 5) & 15; }
        else             { F = C ^ (B | ~D);       g = (7*i) & 15; }
        F = F + A + K[i] + M[g];
        A = D; D = C; C = B;
        B = B + ROTL(F, R[i]);
    }
    c->s[0] += A; c->s[1] += B; c->s[2] += C; c->s[3] += D;
}

void rdl_md5_init(rdl_md5_ctx *c)
{
    c->s[0] = 0x67452301u; c->s[1] = 0xefcdab89u;
    c->s[2] = 0x98badcfeu; c->s[3] = 0x10325476u;
    c->len = 0; c->buflen = 0;
}

void rdl_md5_update(rdl_md5_ctx *c, const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    c->len += n;
    if (c->buflen) {
        size_t take = 64 - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, p, take);
        c->buflen += take; p += take; n -= take;
        if (c->buflen == 64) { transform(c, c->buf); c->buflen = 0; }
    }
    while (n >= 64) { transform(c, p); p += 64; n -= 64; }
    if (n) { memcpy(c->buf, p, n); c->buflen = n; }
}

void rdl_md5_final(rdl_md5_ctx *c, uint8_t out[16])
{
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80, zero = 0, lb[8];
    rdl_md5_update(c, &pad, 1);
    while (c->buflen != 56) rdl_md5_update(c, &zero, 1);
    for (int i = 0; i < 8; i++) lb[i] = (uint8_t)(bits >> (8 * i));
    rdl_md5_update(c, lb, 8);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            out[i*4 + j] = (uint8_t)(c->s[i] >> (8 * j));
}

void rdl_md5_hex(const uint8_t d[16], char out[33])
{
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) { out[i*2] = hx[d[i] >> 4]; out[i*2+1] = hx[d[i] & 15]; }
    out[32] = 0;
}
