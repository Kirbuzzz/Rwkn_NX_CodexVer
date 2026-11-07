/* Utilidades de archivos compartidas (rutas, mkdir -p, tamano, espacio libre). */
#ifndef RDL_FS_H
#define RDL_FS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool rdl_fs_copy_str(char *dst, size_t cap, const char *src);
bool rdl_fs_join(char *out, size_t cap, const char *dir, const char *name);
bool rdl_fs_mkdir_p(const char *path);                 /* acepta "sdmc:/..." */
void rdl_fs_ensure_parent(const char *file_path);      /* crea la carpeta que contiene el archivo */
bool rdl_fs_size(const char *path, uint64_t *out);     /* solo archivos regulares */
bool rdl_fs_free_space(const char *path, uint64_t *out);

#endif
