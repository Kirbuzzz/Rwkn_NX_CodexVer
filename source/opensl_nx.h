/* opensl_nx.h -- salida de audio de la Switch para el emulador de OpenSL ES (SDL2 + hilo mezclador). */
#ifndef __OPENSL_NX_H__
#define __OPENSL_NX_H__

/* Registra el gancho: el dispositivo de audio se abre la primera vez que el juego pone un jugador en PLAYING. */
void opensl_nx_install(void);
void opensl_nx_shutdown(void);

#endif
