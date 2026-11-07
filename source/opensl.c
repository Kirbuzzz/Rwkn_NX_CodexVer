/* opensl.c -- ver opensl.h */
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "opensl.h"

#ifdef __SWITCH__
#include <switch.h>
#include "log.h"
#include "config.h"
#else
#include <pthread.h>
#include <stdio.h>
#define LOG(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
#endif

#define MAX_PLAYERS   16
#define MAX_QUEUE     32
#define MAX_MIX       4096          /* frames por llamada a opensl_mix */

/* ---------------------------------------------------------------- bloqueo */
#ifdef __SWITCH__
static Mutex s_lock;
#define LOCK()   mutexLock(&s_lock)
#define UNLOCK() mutexUnlock(&s_lock)
#else
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK()   pthread_mutex_lock(&s_lock)
#define UNLOCK() pthread_mutex_unlock(&s_lock)
#endif

/* ---------------------------------------------------------------- objetos */

enum { OBJ_ENGINE = 1, OBJ_MIX, OBJ_PLAYER };

typedef struct nx_obj {
  const struct SLObjectItf_ *vt;        /* debe ser el primer campo: SLObjectItf apunta aqui */
  int kind;
  SLuint32 state;
  int alive;
  /* interfaces (cada una empieza por su puntero a tabla) */
  struct { const struct SLEngineItf_ *vt; struct nx_obj *o; } engine;
  struct { const struct SLPlayItf_ *vt; struct nx_obj *o; } play;
  struct { const struct SLAndroidSimpleBufferQueueItf_ *vt; struct nx_obj *o; } bq;
  struct { const struct SLAndroidConfigurationItf_ *vt; struct nx_obj *o; } cfg;

  /* jugador */
  int channels, bits, is_float, rate;
  unsigned bytes_per_frame;
  struct { const uint8_t *data; uint32_t size, pos; } q[MAX_QUEUE];
  int q_head, q_count;
  uint32_t completed;                   /* buffers terminados desde siempre (== "index" de GetState) */
  int playing, pending_cb;
  double frac;                          /* posicion fraccionaria del remuestreo, en frames de origen */
  slAndroidSimpleBufferQueueCallback cb;
  void *cb_ctx;
} nx_obj;

static nx_obj *s_players[MAX_PLAYERS];
static void (*s_start_hook)(void);
static int s_hook_fired;

/* ---- IDs de interfaz (UUIDs de la especificacion; solo importa que sean distintos) ---- */
static const struct SLInterfaceID_ iid_engine_s = { 0x8d97c260, 0xddd4, 0x11db, 0x958f, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } };
static const struct SLInterfaceID_ iid_play_s   = { 0xef0bd9c0, 0xddd4, 0x11db, 0xbf49, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } };
static const struct SLInterfaceID_ iid_bq_s     = { 0x198e4940, 0xc5d7, 0x11dd, 0xad8b, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } };
static const struct SLInterfaceID_ iid_cfg_s    = { 0x89f6a7e0, 0xbeac, 0x11df, 0x8b5c, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } };
const SLInterfaceID SL_IID_ENGINE_nx = &iid_engine_s;
const SLInterfaceID SL_IID_PLAY_nx = &iid_play_s;
const SLInterfaceID SL_IID_BUFFERQUEUE_nx = &iid_bq_s;
const SLInterfaceID SL_IID_ANDROIDCONFIGURATION_nx = &iid_cfg_s;

static int iid_is(SLInterfaceID a, SLInterfaceID b) {
  return a && (a == b || memcmp(a, b, sizeof *a) == 0);
}

/* ----------------------------------------------------------- SLObjectItf */

static SLresult obj_realize(SLObjectItf self, SLboolean async) {
  (void)async;
  nx_obj *o = (nx_obj *)self;
  if (!o) return SL_RESULT_PARAMETER_INVALID;
  o->state = SL_OBJECT_STATE_REALIZED;
  return SL_RESULT_SUCCESS;
}
static SLresult obj_resume(SLObjectItf self, SLboolean async) { (void)self; (void)async; return SL_RESULT_SUCCESS; }
static SLresult obj_get_state(SLObjectItf self, SLuint32 *st) {
  nx_obj *o = (nx_obj *)self;
  if (!o || !st) return SL_RESULT_PARAMETER_INVALID;
  *st = o->state;
  return SL_RESULT_SUCCESS;
}
static SLresult obj_get_interface(SLObjectItf self, SLInterfaceID iid, void *out) {
  nx_obj *o = (nx_obj *)self;
  if (!o || !iid || !out) return SL_RESULT_PARAMETER_INVALID;
  if (o->kind == OBJ_ENGINE && iid_is(iid, SL_IID_ENGINE_nx)) { *(void **)out = &o->engine.vt; return SL_RESULT_SUCCESS; }
  if (o->kind == OBJ_PLAYER) {
    if (iid_is(iid, SL_IID_PLAY_nx))                 { *(void **)out = &o->play.vt; return SL_RESULT_SUCCESS; }
    if (iid_is(iid, SL_IID_BUFFERQUEUE_nx))          { *(void **)out = &o->bq.vt;   return SL_RESULT_SUCCESS; }
    if (iid_is(iid, SL_IID_ANDROIDCONFIGURATION_nx)) { *(void **)out = &o->cfg.vt;  return SL_RESULT_SUCCESS; }
  }
  *(void **)out = NULL;
  return SL_RESULT_FEATURE_UNSUPPORTED;
}
static SLresult obj_register_cb(SLObjectItf self, void *cb, void *ctx) { (void)self; (void)cb; (void)ctx; return SL_RESULT_SUCCESS; }
static void obj_abort(SLObjectItf self) { (void)self; }
static void obj_destroy(SLObjectItf self) {
  nx_obj *o = (nx_obj *)self;
  if (!o) return;
  LOCK();
  o->playing = 0;
  o->alive = 0;
  o->q_count = 0;
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (s_players[i] == o) s_players[i] = NULL;
  UNLOCK();
  /* la memoria no se libera: un hilo de mezcla podria estar dentro de un callback de este jugador */
}
static SLresult obj_set_priority(SLObjectItf self, SLint32 p, SLboolean pre) { (void)self; (void)p; (void)pre; return SL_RESULT_SUCCESS; }
static SLresult obj_get_priority(SLObjectItf self, SLint32 *p, SLboolean *pre) {
  (void)self;
  if (p) *p = 0;
  if (pre) *pre = 0;
  return SL_RESULT_SUCCESS;
}
static SLresult obj_set_loc(SLObjectItf self, SLint16 n, SLInterfaceID *ids, SLboolean en) { (void)self; (void)n; (void)ids; (void)en; return SL_RESULT_SUCCESS; }

static const struct SLObjectItf_ k_obj_vt = {
  obj_realize, obj_resume, obj_get_state, obj_get_interface, obj_register_cb,
  obj_abort, obj_destroy, obj_set_priority, obj_get_priority, obj_set_loc,
};

/* ---------------------------------------------------------------- PLAY */

static SLresult play_set_state(SLPlayItf self, SLuint32 state) {
  nx_obj *o = (nx_obj *)((const char *)self - offsetof(nx_obj, play));
  if (!self) return SL_RESULT_PARAMETER_INVALID;
  if (state < SL_PLAYSTATE_STOPPED || state > SL_PLAYSTATE_PLAYING) return SL_RESULT_PARAMETER_INVALID;
  LOCK();
  o->playing = (state == SL_PLAYSTATE_PLAYING);
  UNLOCK();
  if (o->playing && !s_hook_fired && s_start_hook) { s_hook_fired = 1; s_start_hook(); }
  return SL_RESULT_SUCCESS;
}
static SLresult play_get_state(SLPlayItf self, SLuint32 *st) {
  nx_obj *o = (nx_obj *)((const char *)self - offsetof(nx_obj, play));
  if (!self || !st) return SL_RESULT_PARAMETER_INVALID;
  *st = o->playing ? SL_PLAYSTATE_PLAYING : SL_PLAYSTATE_STOPPED;
  return SL_RESULT_SUCCESS;
}
static SLresult play_zero(SLPlayItf self, SLuint32 *p) { (void)self; if (p) *p = 0; return SL_RESULT_SUCCESS; }
static SLresult play_reg_cb(SLPlayItf self, void *cb, void *ctx) { (void)self; (void)cb; (void)ctx; return SL_RESULT_SUCCESS; }
static SLresult play_set_mask(SLPlayItf self, SLuint32 m) { (void)self; (void)m; return SL_RESULT_SUCCESS; }
static SLresult play_set_pos(SLPlayItf self, SLuint32 m) { (void)self; (void)m; return SL_RESULT_SUCCESS; }
static SLresult play_clear(SLPlayItf self) { (void)self; return SL_RESULT_SUCCESS; }

static const struct SLPlayItf_ k_play_vt = {
  play_set_state, play_get_state, play_zero, play_zero, play_reg_cb, play_set_mask, play_zero,
  play_set_pos, play_clear, play_zero, play_set_pos, play_zero,
};

/* ---------------------------------------------------------- cola de buffers */

static nx_obj *bq_obj(SLAndroidSimpleBufferQueueItf self) { return (nx_obj *)((const char *)self - offsetof(nx_obj, bq)); }

static SLresult bq_enqueue(SLAndroidSimpleBufferQueueItf self, const void *buf, SLuint32 size) {
  if (!self || !buf) return SL_RESULT_PARAMETER_INVALID;
  nx_obj *o = bq_obj(self);
  SLresult r = SL_RESULT_SUCCESS;
  LOCK();
  if (o->q_count >= MAX_QUEUE) {
    r = SL_RESULT_BUFFER_INSUFFICIENT;
  } else {
    const int slot = (o->q_head + o->q_count) % MAX_QUEUE;
    o->q[slot].data = buf;
    o->q[slot].size = size;
    o->q[slot].pos = 0;
    o->q_count++;
  }
  UNLOCK();
  return r;
}
static SLresult bq_clear(SLAndroidSimpleBufferQueueItf self) {
  if (!self) return SL_RESULT_PARAMETER_INVALID;
  nx_obj *o = bq_obj(self);
  LOCK();
  o->q_count = 0;
  o->frac = 0;
  UNLOCK();
  return SL_RESULT_SUCCESS;
}
static SLresult bq_get_state(SLAndroidSimpleBufferQueueItf self, SLAndroidSimpleBufferQueueState *st) {
  if (!self || !st) return SL_RESULT_PARAMETER_INVALID;
  nx_obj *o = bq_obj(self);
  LOCK();
  st->count = (SLuint32)o->q_count;
  st->index = o->completed;
  UNLOCK();
  return SL_RESULT_SUCCESS;
}
static SLresult bq_register_cb(SLAndroidSimpleBufferQueueItf self, slAndroidSimpleBufferQueueCallback cb, void *ctx) {
  if (!self) return SL_RESULT_PARAMETER_INVALID;
  nx_obj *o = bq_obj(self);
  LOCK();
  o->cb = cb;
  o->cb_ctx = ctx;
  UNLOCK();
  return SL_RESULT_SUCCESS;
}
static const struct SLAndroidSimpleBufferQueueItf_ k_bq_vt = { bq_enqueue, bq_clear, bq_get_state, bq_register_cb };

/* ------------------------------------------------------- config. Android */

static SLresult cfg_set(SLAndroidConfigurationItf self, const void *k, const void *v, SLuint32 n) { (void)self; (void)k; (void)v; (void)n; return SL_RESULT_SUCCESS; }
static SLresult cfg_get(SLAndroidConfigurationItf self, const void *k, SLuint32 *n, void *v) { (void)self; (void)k; (void)v; if (n) *n = 0; return SL_RESULT_SUCCESS; }
static const struct SLAndroidConfigurationItf_ k_cfg_vt = { cfg_set, cfg_get };

/* ---------------------------------------------------------------- motor */

static SLresult eng_create_mix(SLEngineItf self, SLObjectItf *pMix, SLuint32 n, const SLInterfaceID *ids, const SLboolean *req);
static SLresult eng_create_player(SLEngineItf self, SLObjectItf *pPlayer, SLDataSource *src, SLDataSink *snk,
                                  SLuint32 n, const SLInterfaceID *ids, const SLboolean *req);

static const struct SLEngineItf_ k_eng_vt = {
  .CreateAudioPlayer = eng_create_player,
  .CreateOutputMix = eng_create_mix,
};

static nx_obj *new_obj(int kind) {
  nx_obj *o = calloc(1, sizeof *o);
  if (!o) return NULL;
  o->vt = &k_obj_vt;
  o->kind = kind;
  o->state = SL_OBJECT_STATE_UNREALIZED;
  o->alive = 1;
  o->engine.vt = &k_eng_vt; o->play.vt = &k_play_vt; o->bq.vt = &k_bq_vt; o->cfg.vt = &k_cfg_vt;
  o->engine.o = o; o->play.o = o; o->bq.o = o; o->cfg.o = o;
  return o;
}

static SLresult eng_create_mix(SLEngineItf self, SLObjectItf *pMix, SLuint32 n, const SLInterfaceID *ids, const SLboolean *req) {
  (void)self; (void)n; (void)ids; (void)req;
  if (!pMix) return SL_RESULT_PARAMETER_INVALID;
  nx_obj *o = new_obj(OBJ_MIX);
  if (!o) return SL_RESULT_MEMORY_FAILURE;
  *pMix = (SLObjectItf)o;
  return SL_RESULT_SUCCESS;
}

static SLresult eng_create_player(SLEngineItf self, SLObjectItf *pPlayer, SLDataSource *src, SLDataSink *snk,
                                  SLuint32 n, const SLInterfaceID *ids, const SLboolean *req) {
  (void)self; (void)snk; (void)n; (void)ids; (void)req;
  if (!pPlayer || !src || !src->pLocator || !src->pFormat) return SL_RESULT_PARAMETER_INVALID;
  if (*(SLuint32 *)src->pLocator != SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE) return SL_RESULT_FEATURE_UNSUPPORTED;

  const SLuint32 ftype = *(SLuint32 *)src->pFormat;
  int ch, bits, is_float = 0, rate;
  if (ftype == SL_DATAFORMAT_PCM) {
    SLDataFormat_PCM *f = src->pFormat;
    ch = (int)f->numChannels; bits = (int)f->bitsPerSample; rate = (int)(f->samplesPerSec >= 1000000u ? f->samplesPerSec / 1000u : f->samplesPerSec);
  } else if (ftype == SL_ANDROID_DATAFORMAT_PCM_EX) {
    SLAndroidDataFormat_PCM_EX *f = src->pFormat;
    ch = (int)f->numChannels; bits = (int)f->bitsPerSample; rate = (int)(f->sampleRate >= 1000000u ? f->sampleRate / 1000u : f->sampleRate);
    is_float = (f->representation == SL_ANDROID_PCM_REPRESENTATION_FLOAT);
  } else {
    return SL_RESULT_FEATURE_UNSUPPORTED;
  }
  if (ch < 1 || ch > 8 || rate < 4000 || rate > 192000) return SL_RESULT_PARAMETER_INVALID;
  if (!((bits == 8 || bits == 16 || bits == 32) && !is_float) && !(is_float && bits == 32)) return SL_RESULT_FEATURE_UNSUPPORTED;

  nx_obj *o = new_obj(OBJ_PLAYER);
  if (!o) return SL_RESULT_MEMORY_FAILURE;
  o->channels = ch; o->bits = bits; o->is_float = is_float; o->rate = rate;
  o->bytes_per_frame = (unsigned)(ch * bits / 8);

  LOCK();
  int placed = 0;
  for (int i = 0; i < MAX_PLAYERS && !placed; i++)
    if (!s_players[i]) { s_players[i] = o; placed = 1; }
  UNLOCK();
  if (!placed) { free(o); return SL_RESULT_MEMORY_FAILURE; }

  *pPlayer = (SLObjectItf)o;
  return SL_RESULT_SUCCESS;
}

SLresult slCreateEngine_nx(SLObjectItf *pEngine, SLuint32 numOptions, const void *opts,
                           SLuint32 numInterfaces, const SLInterfaceID *ids, const SLboolean *req) {
  (void)numOptions; (void)opts; (void)numInterfaces; (void)ids; (void)req;
  if (!pEngine) return SL_RESULT_PARAMETER_INVALID;
  nx_obj *o = new_obj(OBJ_ENGINE);
  if (!o) return SL_RESULT_MEMORY_FAILURE;
  *pEngine = (SLObjectItf)o;
  return SL_RESULT_SUCCESS;
}

/* ------------------------------------------------------------- mezclador */

static inline float read_sample(const uint8_t *p, int bits, int is_float) {
  if (is_float) { float f; memcpy(&f, p, 4); return f; }
  if (bits == 16) { int16_t s; memcpy(&s, p, 2); return (float)s * (1.0f / 32768.0f); }
  if (bits == 8) return ((float)((int)p[0] - 128)) * (1.0f / 128.0f);
  int32_t s; memcpy(&s, p, 4); return (float)s * (1.0f / 2147483648.0f);
}

/* Mezcla hasta `need` frames de salida del jugador p en acc (estereo float). Devuelve cuantos produjo.
 * Debe llamarse con el bloqueo tomado. Anota en p->pending_cb los buffers terminados. */
static int mix_player(nx_obj *p, float *acc, int need) {
  const double step = (double)p->rate / (double)OPENSL_OUT_RATE;
  int produced = 0;
  while (produced < need && p->q_count > 0) {
    typeof(p->q[0]) *b = &p->q[p->q_head];
    const uint32_t frames_in_buf = b->size / p->bytes_per_frame;
    while (produced < need) {
      const uint32_t idx = (uint32_t)(b->pos);                     /* frame entero actual dentro del buffer */
      if (idx >= frames_in_buf) break;
      const uint8_t *fr0 = b->data + (size_t)idx * p->bytes_per_frame;
      const uint8_t *fr1 = (idx + 1 < frames_in_buf) ? fr0 + p->bytes_per_frame : fr0;
      const int sb = p->bits / 8;
      float l0 = read_sample(fr0, p->bits, p->is_float), r0 = p->channels > 1 ? read_sample(fr0 + sb, p->bits, p->is_float) : l0;
      float l1 = read_sample(fr1, p->bits, p->is_float), r1 = p->channels > 1 ? read_sample(fr1 + sb, p->bits, p->is_float) : l1;
      const float t = (float)p->frac;
      acc[produced * 2]     += l0 + (l1 - l0) * t;
      acc[produced * 2 + 1] += r0 + (r1 - r0) * t;
      produced++;
      p->frac += step;
      const uint32_t adv = (uint32_t)p->frac;
      p->frac -= adv;
      b->pos += adv;
    }
    if (b->pos >= frames_in_buf) {                                 /* buffer agotado */
      p->q_head = (p->q_head + 1) % MAX_QUEUE;
      p->q_count--;
      p->completed++;
      p->pending_cb++;
      p->frac = 0;
    }
  }
  return produced;
}

void opensl_mix(int16_t *out, int frames) {
  static float acc[MAX_MIX * 2];
  if (frames > MAX_MIX) frames = MAX_MIX;
  memset(acc, 0, (size_t)frames * 2 * sizeof(float));

  for (int i = 0; i < MAX_PLAYERS; i++) {
    int done = 0;
    for (int pass = 0; pass < 3 && done < frames; pass++) {
      LOCK();
      nx_obj *p = s_players[i];
      slAndroidSimpleBufferQueueCallback cb = NULL;
      void *ctx = NULL;
      int ncb = 0;
      if (p && p->alive && p->playing) {
        done += mix_player(p, acc + done * 2, frames - done);
        ncb = p->pending_cb;
        p->pending_cb = 0;
        cb = p->cb;
        ctx = p->cb_ctx;
      }
      UNLOCK();
      if (!ncb) break;
      /* el callback del juego encola el siguiente buffer; se llama sin el bloqueo tomado */
      if (cb) for (int k = 0; k < ncb; k++) cb((SLAndroidSimpleBufferQueueItf)&p->bq.vt, ctx);
    }
  }

  for (int i = 0; i < frames * 2; i++) {
    float v = acc[i];
    if (v > 1.0f) v = 1.0f; else if (v < -1.0f) v = -1.0f;
    out[i] = (int16_t)lrintf(v * 32767.0f);
  }
}

int opensl_playing_count(void) {
  int n = 0;
  LOCK();
  for (int i = 0; i < MAX_PLAYERS; i++) if (s_players[i] && s_players[i]->playing) n++;
  UNLOCK();
  return n;
}

void opensl_set_start_hook(void (*hook)(void)) { s_start_hook = hook; }
