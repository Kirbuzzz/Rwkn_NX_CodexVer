/* opensl_nx.c -- ver opensl_nx.h. Un dispositivo SDL2 en modo "push" (SDL_QueueAudio); el hilo mezclador es
 * el unico productor: llama a opensl_mix() (que invoca los callbacks del juego) y aplica contrapresion para
 * que el motor de audio vaya al ritmo del DAC, como con un AudioTrack bloqueante. */

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <switch.h>

#include "log.h"
#include "opensl.h"
#include "opensl_nx.h"
#include "pthr.h"

#define CHUNK_FRAMES 512

static SDL_AudioDeviceID s_dev;
static Thread s_thread;
static volatile int s_run;
static int s_started;
static Uint32 s_queue_cap;     /* techo de contrapresion, en bytes */

static void mixer_main(void *arg) {
  (void)arg;
  pthr_ensure_fake_tls();       /* este hilo va a ejecutar codigo del juego (los callbacks de la cola) */
  pthr_pin_bg_core();

  int16_t buf[CHUNK_FRAMES * 2];
  while (s_run) {
    while (s_run && SDL_GetQueuedAudioSize(s_dev) > s_queue_cap)
      svcSleepThread(500000ull); /* 0.5 ms */
    if (!s_run) break;
    opensl_mix(buf, CHUNK_FRAMES);
    SDL_QueueAudio(s_dev, buf, sizeof buf);
  }
}

static void start_audio(void) {
  if (s_started)
    return;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
    LOG("audio: SDL_InitSubSystem fallo: %s", SDL_GetError());
    return;
  }
  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = OPENSL_OUT_RATE;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = NULL;         /* modo push */
  s_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (s_dev == 0) {
    LOG("audio: SDL_OpenAudioDevice fallo: %s", SDL_GetError());
    return;
  }
  s_queue_cap = (have.size ? have.size : 4096u) * 4u;
  SDL_PauseAudioDevice(s_dev, 0);

  s_run = 1;
  if (R_FAILED(threadCreate(&s_thread, mixer_main, NULL, NULL, 0x10000, 0x20, -2)) ||
      R_FAILED(threadStart(&s_thread))) {
    LOG("audio: no se pudo crear el hilo mezclador");
    s_run = 0;
    return;
  }
  s_started = 1;
  LOG("audio: salida abierta (%d Hz, %d ch)", have.freq, have.channels);
}

void opensl_nx_install(void) {
  opensl_set_start_hook(start_audio);
}

void opensl_nx_shutdown(void) {
  if (!s_started)
    return;
  s_run = 0;
  threadWaitForExit(&s_thread);
  threadClose(&s_thread);
  SDL_CloseAudioDevice(s_dev);
  s_started = 0;
}
