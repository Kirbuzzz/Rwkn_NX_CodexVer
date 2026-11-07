/* aasset.h -- AAssetManager del NDK, emulado sobre la carpeta assets/ extraida del apk. */
#ifndef __AASSET_H__
#define __AASSET_H__

#include <stddef.h>
#include <sys/types.h>

void *AAssetManager_fromJava_nx(void *env, void *asset_manager);
void *AAssetManager_open_nx(void *mgr, const char *filename, int mode);
off_t AAsset_getLength_nx(void *asset);
int   AAsset_read_nx(void *asset, void *buf, size_t count);
off_t AAsset_seek_nx(void *asset, off_t offset, int whence);
void  AAsset_close_nx(void *asset);

#endif
