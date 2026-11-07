#ifndef REAWAKEN_NX_MODMENU_H
#define REAWAKEN_NX_MODMENU_H

#include <stdbool.h>

/* Menu nativo independiente de las APIs Android del mod original. */
bool modmenu_init(int width, int height);
void modmenu_exit(void);
/* Lee el mando. L+R+Abajo abre/cierra; B cierra; arriba/abajo cambia selección. */
bool modmenu_update(void);
/* Dibuja el panel encima del frame del juego, sin hacer swap. */
void modmenu_render(void);
bool modmenu_is_open(void);

#endif
