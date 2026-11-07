/* rdl: descargador del obb de Reawaken para Switch (libnx + libcurl).
 *
 * Uso tipico:
 *   socketInitializeDefault();                 // lo hace la app, antes de rdl_init
 *   rdl_init(&cfg);
 *   rdl_fetch_manifest(&m, ".../data/manifest.cache.json");
 *   if (rdl_obb_installed(&m, obb_dir) == RDL_OBB_NONE)
 *       rdl_obb_download(&m.normal, obb_dir, progreso, NULL);   // o &m.hd
 *   rdl_exit();
 */
#ifndef RDL_H
#define RDL_H
#include <stdbool.h>
#include <stdint.h>
#include "rdl_manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RDL_OK            =  0,
    RDL_ERR_ARG       = -1,
    RDL_ERR_INIT      = -2,
    RDL_ERR_NET       = -3,   /* sin red, timeout o corte: reintentar, la descarga se reanuda */
    RDL_ERR_HTTP      = -4,   /* el servidor respondio con error */
    RDL_ERR_MANIFEST  = -5,
    RDL_ERR_IO        = -6,   /* error de lectura/escritura en la SD */
    RDL_ERR_SPACE     = -7,   /* no hay espacio libre suficiente */
    RDL_ERR_MD5       = -8,   /* el archivo no coincide con el manifest */
    RDL_ERR_CANCELLED = -9,
    RDL_ERR_APK       = -10   /* apk ausente de lo esperado, zip invalido o dañado */
} rdl_result;

typedef struct {
    const char *base_url;     /* "https://reawaken-lb.throwaway09728.workers.dev" (solo https) */
    const char *ca_file;      /* ruta a cacert.pem (en Switch hace falta); NULL = defecto de curl */
    const char *user_agent;   /* NULL = "Reawaken" */
    bool        insecure_tls; /* true = no validar certificado (solo para depurar) */
    const char *ca_pem;       /* bundle de CA embebido (rdl_cacert_pem); se usa si no hay ca_file en la SD */
    unsigned    ca_pem_len;
} rdl_config;

extern const char   rdl_cacert_pem[];       /* src/rdl_cacert.c (generado por tools/gen_cacert.py) */
extern const unsigned rdl_cacert_pem_len;

typedef enum { RDL_PHASE_DOWNLOAD, RDL_PHASE_VERIFY, RDL_PHASE_EXTRACT } rdl_phase;

/* Devuelve false para cancelar. En descarga, `done` incluye lo que ya habia en el .part. */
typedef bool (*rdl_progress_fn)(rdl_phase phase, uint64_t done, uint64_t total, void *user);

typedef enum {
    RDL_OBB_NONE,             /* no hay obb en la carpeta */
    RDL_OBB_NORMAL,           /* el tamano coincide con el obb normal */
    RDL_OBB_HD,               /* el tamano coincide con el obb HD */
    RDL_OBB_OTHER             /* hay un archivo con ese nombre pero no coincide (version vieja/corrupto) */
} rdl_obb_kind;

rdl_result  rdl_init(const rdl_config *cfg);
void        rdl_exit(void);

/* Descarga el manifest. Si falla la red y cache_path existe, usa la copia local (from_cache = true). */
rdl_result  rdl_fetch_manifest(rdl_manifest *out, const char *cache_path);

/* Comprobacion rapida (solo tamano, sin red). */
rdl_obb_kind rdl_obb_installed(const rdl_manifest *m, const char *obb_dir);

/* Comprobacion completa (tamano + MD5). RDL_OK si el obb instalado coincide con `info`. */
rdl_result  rdl_obb_verify(const rdl_obb_info *info, const char *obb_dir, rdl_progress_fn cb, void *user);

/* Descarga `info` en obb_dir: reanuda el .part, valida MD5 y recien ahi reemplaza el obb existente.
 * Si ya esta instalado y es correcto, no descarga nada. */
rdl_result  rdl_obb_download(const rdl_obb_info *info, const char *obb_dir, rdl_progress_fn cb, void *user);

const char *rdl_strerror(rdl_result r);

/* Detalle del ultimo error de red/manifest (p. ej. "curl 60: SSL certificate problem"). Vacio si no hubo. */
const char *rdl_last_error(void);

#ifdef __cplusplus
}
#endif
#endif
