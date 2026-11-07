/* Lectura del manifest.json de Reawaken (parser JSON minimo, sin dependencias). */
#ifndef RDL_MANIFEST_H
#define RDL_MANIFEST_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RDL_NAME_MAX 128
#define RDL_URL_MAX  512

typedef struct {
    bool     valid;
    char     file_name[RDL_NAME_MAX];
    char     url[RDL_URL_MAX];
    uint64_t size;
    char     md5[33];            /* hex en minusculas */
} rdl_obb_info;

typedef struct {
    rdl_obb_info normal;         /* reawaken.obb   */
    rdl_obb_info hd;             /* reawaken.obbHd (opcional: hd.valid == false si falta) */
    rdl_obb_info apk;            /* reawaken.apk (opcional); file_name = "Reawaken.apk" */
    bool         from_cache;     /* true si se leyo de la copia local por falta de red */
} rdl_manifest;

/* 0 = ok; -1 = JSON invalido o sin "reawaken"; -2 = falta "obb" o es invalido. */
int rdl_manifest_parse(const char *json, size_t len, rdl_manifest *out);

#endif
