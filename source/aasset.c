/* aasset.c -- ver aasset.h. Los assets los extrae rdl_apk_extract() a <base>/assets/. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aasset.h"
#include "config.h"
#include "log.h"
#include "paths.h"

typedef struct {
  FILE *f;
  off_t len;
} NxAsset;

static int s_dummy_manager;   /* el motor solo necesita un puntero no nulo */

void *AAssetManager_fromJava_nx(void *env, void *asset_manager) {
  (void)env; (void)asset_manager;
  return &s_dummy_manager;
}

void *AAssetManager_open_nx(void *mgr, const char *filename, int mode) {
  (void)mgr; (void)mode;
  if (!filename || !filename[0])
    return NULL;
  /* sin rutas absolutas ni ".." (el motor pasa rutas relativas a assets/) */
  if (filename[0] == '/' || strstr(filename, ".."))
    return NULL;

  char path[640];
  snprintf(path, sizeof path, "%s/%s", path_assets(), filename);
  FILE *f = fopen(path, "rb");
  if (!f) {
    if (config.log_imports) LOG("AAsset: no existe '%s'", filename);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  const long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  NxAsset *a = malloc(sizeof *a);
  if (!a) { fclose(f); return NULL; }
  a->f = f;
  a->len = len < 0 ? 0 : (off_t)len;
  return a;
}

off_t AAsset_getLength_nx(void *asset) {
  return asset ? ((NxAsset *)asset)->len : 0;
}

int AAsset_read_nx(void *asset, void *buf, size_t count) {
  if (!asset || !buf) return -1;
  return (int)fread(buf, 1, count, ((NxAsset *)asset)->f);
}

off_t AAsset_seek_nx(void *asset, off_t offset, int whence) {
  if (!asset) return -1;
  NxAsset *a = asset;
  if (fseek(a->f, (long)offset, whence) != 0) return -1;
  return (off_t)ftell(a->f);
}

void AAsset_close_nx(void *asset) {
  if (!asset) return;
  NxAsset *a = asset;
  fclose(a->f);
  free(a);
}
