/* Simula la secuencia de Wwise (AkSink OpenSL) contra el emulador y comprueba la mezcla. */
#include "../source/opensl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("  FALLO linea %d: %s\n", __LINE__, #c); } } while (0)

typedef struct {
  SLObjectItf engine, mix, player;
  SLEngineItf eng;
  SLPlayItf play;
  SLAndroidSimpleBufferQueueItf bq;
  SLAndroidConfigurationItf cfg;
} Rig;

static int16_t bufs[4][480 * 2];    /* 4 buffers de 480 frames estereo */
static int next_buf = 0, cb_count = 0, cb_ok = 0;
static void *last_ctx;

static void gen(int16_t *b, int n, int seed) { for (int i = 0; i < n; i++) { b[2*i] = (int16_t)(1000 + seed * 100 + i); b[2*i+1] = (int16_t)(-1000 - seed * 100 - i); } }

static void on_buf(SLAndroidSimpleBufferQueueItf caller, void *ctx) {
  cb_count++;
  last_ctx = ctx;
  Rig *r = ctx;
  cb_ok += (caller == r->bq);
  const int i = next_buf++ % 4;
  gen(bufs[i], 480, 10 + next_buf);
  (*caller)->Enqueue(caller, bufs[i], sizeof bufs[i]);       /* el juego encola dentro del callback */
}

static void build(Rig *r, SLDataSource *src, SLDataSink *snk) {
  SLEngineItf eng;
  CHECK(slCreateEngine_nx(&r->engine, 0, NULL, 0, NULL, NULL) == SL_RESULT_SUCCESS);
  CHECK((*r->engine)->Realize(r->engine, 0) == SL_RESULT_SUCCESS);
  CHECK((*r->engine)->GetInterface(r->engine, SL_IID_ENGINE_nx, &eng) == SL_RESULT_SUCCESS && eng);
  r->eng = eng;
  CHECK((*eng)->CreateOutputMix(eng, &r->mix, 0, NULL, NULL) == SL_RESULT_SUCCESS);
  CHECK((*r->mix)->Realize(r->mix, 0) == SL_RESULT_SUCCESS);
  const SLInterfaceID ids[2] = { SL_IID_BUFFERQUEUE_nx, SL_IID_ANDROIDCONFIGURATION_nx };
  const SLboolean req[2] = { 1, 1 };
  CHECK((*eng)->CreateAudioPlayer(eng, &r->player, src, snk, 2, ids, req) == SL_RESULT_SUCCESS);
  CHECK((*r->player)->Realize(r->player, 0) == SL_RESULT_SUCCESS);
  CHECK((*r->player)->GetInterface(r->player, SL_IID_PLAY_nx, &r->play) == SL_RESULT_SUCCESS);
  CHECK((*r->player)->GetInterface(r->player, SL_IID_BUFFERQUEUE_nx, &r->bq) == SL_RESULT_SUCCESS);
  CHECK((*r->player)->GetInterface(r->player, SL_IID_ANDROIDCONFIGURATION_nx, &r->cfg) == SL_RESULT_SUCCESS);
  CHECK((*r->cfg)->SetConfiguration(r->cfg, "streamType", "x", 1) == SL_RESULT_SUCCESS);
  CHECK((*r->bq)->RegisterCallback(r->bq, on_buf, r) == SL_RESULT_SUCCESS);
}

int main(void) {
  SLDataLocator_AndroidSimpleBufferQueue loc = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 4 };
  SLDataFormat_PCM fmt = { SL_DATAFORMAT_PCM, 2, 48000000, 16, 16, 3, 2 };
  SLDataSource src = { &loc, &fmt };
  SLDataSink snk = { NULL, NULL };
  Rig r;
  int16_t out[1200 * 2];

  puts("[secuencia tipo Wwise, PCM16 estereo 48 kHz]");
  build(&r, &src, &snk);
  gen(bufs[0], 480, 1); gen(bufs[1], 480, 2); next_buf = 2;
  CHECK((*r.bq)->Enqueue(r.bq, bufs[0], sizeof bufs[0]) == SL_RESULT_SUCCESS);
  CHECK((*r.bq)->Enqueue(r.bq, bufs[1], sizeof bufs[1]) == SL_RESULT_SUCCESS);
  SLAndroidSimpleBufferQueueState st;
  CHECK((*r.bq)->GetState(r.bq, &st) == SL_RESULT_SUCCESS && st.count == 2 && st.index == 0);

  opensl_mix(out, 256);                                  /* sin PLAYING: silencio */
  { int z = 1; for (int i = 0; i < 512; i++) z &= out[i] == 0; CHECK(z); }
  CHECK(cb_count == 0 && opensl_playing_count() == 0);

  CHECK((*r.play)->SetPlayState(r.play, SL_PLAYSTATE_PLAYING) == SL_RESULT_SUCCESS);
  SLuint32 ps = 0; CHECK((*r.play)->GetPlayState(r.play, &ps) == SL_RESULT_SUCCESS && ps == SL_PLAYSTATE_PLAYING);
  CHECK(opensl_playing_count() == 1);

  opensl_mix(out, 480);                                  /* consume justo el primer buffer */
  CHECK(out[0] == 1100 && out[1] == -1100 && out[2] == 1101 && out[958] == 1100 + 479);
  CHECK(cb_count == 1 && cb_ok == 1 && last_ctx == &r);
  (*r.bq)->GetState(r.bq, &st);
  CHECK(st.index == 1 && st.count == 2);                 /* 1 terminado; quedaba el 2.o + el que encolo el callback */

  opensl_mix(out, 700);                                  /* cruza el 2.o buffer y entra en el encolado por el callback */
  CHECK(out[0] == 1200 && out[2 * 479] == 1200 + 479);  /* 2.o buffer completo */
  CHECK(out[2 * 480] == 1000 + 13 * 100 + 0);            /* arranca el buffer que encolo el 1.er callback (seed 10+3) */
  CHECK(cb_count >= 2);
  { int ok = 1; for (int i = 0; i < 700; i++) ok &= (out[2*i] > 0 && out[2*i+1] < 0); CHECK(ok); }   /* sin huecos */

  puts("[subflujo: sin callbacks nuevos queda en silencio]");
  CHECK((*r.bq)->RegisterCallback(r.bq, NULL, NULL) == SL_RESULT_SUCCESS);
  { int guard = 0; do { opensl_mix(out, 1200); } while (++guard < 20 && out[2 * 1199] != 0); }
  opensl_mix(out, 300); { int z = 1; for (int i = 0; i < 600; i++) z &= out[i] == 0; CHECK(z); }

  puts("[Clear / Destroy]");
  gen(bufs[0], 480, 1);
  CHECK((*r.bq)->Enqueue(r.bq, bufs[0], sizeof bufs[0]) == SL_RESULT_SUCCESS);
  CHECK((*r.bq)->Clear(r.bq) == SL_RESULT_SUCCESS);
  (*r.bq)->GetState(r.bq, &st); CHECK(st.count == 0);
  CHECK((*r.play)->SetPlayState(r.play, SL_PLAYSTATE_PAUSED) == SL_RESULT_SUCCESS && opensl_playing_count() == 0);
  (*r.player)->Destroy(r.player); (*r.mix)->Destroy(r.mix); (*r.engine)->Destroy(r.engine);
  CHECK(opensl_playing_count() == 0);

  puts("[float + mono + 24 kHz (remuestreo x2)]");
  { SLAndroidDataFormat_PCM_EX f2 = { SL_ANDROID_DATAFORMAT_PCM_EX, 1, 24000000, 32, 32, 4, 2, SL_ANDROID_PCM_REPRESENTATION_FLOAT };
    SLDataSource s2 = { &loc, &f2 }; Rig r2; build(&r2, &s2, &snk);
    (*r2.bq)->RegisterCallback(r2.bq, NULL, NULL);
    static float mono[240]; for (int i = 0; i < 240; i++) mono[i] = (float)i / 240.0f;  /* rampa 0..1 */
    CHECK((*r2.bq)->Enqueue(r2.bq, mono, sizeof mono) == SL_RESULT_SUCCESS);
    (*r2.play)->SetPlayState(r2.play, SL_PLAYSTATE_PLAYING);
    opensl_mix(out, 480);                                /* 240 frames a 24 kHz = 480 frames a 48 kHz */
    CHECK(out[0] == 0 && out[0] == out[1]);              /* mono -> ambos canales iguales */
    CHECK(abs(out[2 * 240] - (int)(0.5f * 32767)) < 80);  /* a mitad ~0.5 (interpolacion lineal) */
    CHECK(out[2 * 100 + 2] >= out[2 * 100]);             /* creciente */
    (*r2.player)->Destroy(r2.player); }

  puts("[dos jugadores: suma y recorte]");
  { SLDataFormat_PCM f3 = { SL_DATAFORMAT_PCM, 2, 48000000, 16, 16, 3, 2 }; SLDataSource s3 = { &loc, &f3 };
    Rig a, b; build(&a, &s3, &snk); build(&b, &s3, &snk);
    (*a.bq)->RegisterCallback(a.bq, NULL, NULL); (*b.bq)->RegisterCallback(b.bq, NULL, NULL);
    static int16_t x[200 * 2], y[200 * 2];
    for (int i = 0; i < 400; i++) { x[i] = 20000; y[i] = 20000; }
    (*a.bq)->Enqueue(a.bq, x, sizeof x); (*b.bq)->Enqueue(b.bq, y, sizeof y);
    (*a.play)->SetPlayState(a.play, SL_PLAYSTATE_PLAYING); (*b.play)->SetPlayState(b.play, SL_PLAYSTATE_PLAYING);
    CHECK(opensl_playing_count() == 2);
    opensl_mix(out, 200);
    CHECK(out[0] == 32767 && out[399] == 32767);         /* 20000+20000 recorta, no desborda */ }

  puts("[parametros invalidos]");
  { SLObjectItf e; CHECK(slCreateEngine_nx(NULL, 0, NULL, 0, NULL, NULL) == SL_RESULT_PARAMETER_INVALID);
    slCreateEngine_nx(&e, 0, NULL, 0, NULL, NULL); (*e)->Realize(e, 0);
    SLEngineItf eng; (*e)->GetInterface(e, SL_IID_ENGINE_nx, &eng);
    SLObjectItf p; SLDataLocator_AndroidSimpleBufferQueue bad_loc = { 0x1234, 1 };
    SLDataFormat_PCM f = { SL_DATAFORMAT_PCM, 2, 48000000, 16, 16, 3, 2 }; SLDataSource s = { &bad_loc, &f }; SLDataSink k = { NULL, NULL };
    CHECK((*eng)->CreateAudioPlayer(eng, &p, &s, &k, 0, NULL, NULL) == SL_RESULT_FEATURE_UNSUPPORTED);
    SLDataFormat_PCM f9 = { SL_DATAFORMAT_PCM, 99, 48000000, 16, 16, 3, 2 }; SLDataSource s9 = { &loc, &f9 };
    CHECK((*eng)->CreateAudioPlayer(eng, &p, &s9, &k, 0, NULL, NULL) == SL_RESULT_PARAMETER_INVALID);
    void *none; CHECK((*e)->GetInterface(e, SL_IID_PLAY_nx, &none) == SL_RESULT_FEATURE_UNSUPPORTED && none == NULL); }

  printf("\n%d comprobaciones, %d fallos\n", checks, fails);
  return fails ? 1 : 0;
}
