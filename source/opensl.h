/* opensl.h -- OpenSL ES emulado: lo justo para el sumidero de audio de Wwise (motor UAF).
 *
 * Soporta: slCreateEngine, SL_IID_ENGINE / PLAY / BUFFERQUEUE / ANDROIDCONFIGURATION, un mezclador de
 * jugadores con cola de buffers (PCM 8/16/32 bits entero o float, 1-2 canales, cualquier frecuencia).
 * La salida se genera con opensl_mix() desde un hilo (opensl_nx.c en la Switch). */
#ifndef __OPENSL_H__
#define __OPENSL_H__

#include <stdint.h>
#include "opensl_types.h"

#define OPENSL_OUT_RATE 48000

/* ---- simbolos que se exportan al juego (imports.c) ---- */
SLresult slCreateEngine_nx(SLObjectItf *pEngine, SLuint32 numOptions, const void *pEngineOptions,
                           SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired);
extern const SLInterfaceID SL_IID_ENGINE_nx, SL_IID_PLAY_nx, SL_IID_BUFFERQUEUE_nx, SL_IID_ANDROIDCONFIGURATION_nx;

/* Mezcla `frames` frames estereo S16 @ OPENSL_OUT_RATE en out (siempre llena el buffer; silencio si no hay datos).
 * Invoca los callbacks de las colas ya consumidas, asi que debe llamarse desde un hilo con TLS falso instalado. */
void opensl_mix(int16_t *out, int frames);

/* Cuantos jugadores estan reproduciendo (diagnostico/pruebas). */
int opensl_playing_count(void);

/* Gancho opcional: se llama la primera vez que un jugador pasa a PLAYING (para abrir el dispositivo de audio). */
void opensl_set_start_hook(void (*hook)(void));

#endif
