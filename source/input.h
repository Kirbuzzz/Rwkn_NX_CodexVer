/* input.h -- mando y pantalla tactil para el motor UAF.
 *
 * - Tactil: se empuja al motor con UAFJNILib.touch(prevX, prevY, x, y, 0, puntero, accion)
 *   accion: 0 = abajo, 1 = arriba, 2 = mover (las dos primeras son la posicion anterior), 3 = cancelar.
 * - Mando: el motor CONSULTA a Java cada frame (GameActivity.isGamePadConnected / getGamePadAxisValues /
 *   isGamePadKeyPressed); jni_fake.c reenvia esas consultas a input_pad_axis() / input_key_pressed().
 * - Acoplado sin pantalla tactil: el stick derecho mueve un cursor y R3 hace "toque" en el. */
#ifndef __INPUT_H__
#define __INPUT_H__

typedef struct {
  void (*touch)(void *env, void *cls, float x, float y, float x2, float y2, int zero, int pointer, int action);
  void (*pad_axis)(void *env, void *cls, int index, float x, float y);
  void (*pad_connected)(void *env, void *cls, int connected);
  void (*back_pressed)(void *env, void *cls);
} InputSink;

void input_init(int screen_w, int screen_h, const InputSink *sink);
void input_set_game_input_blocked(int blocked);

/* Lee mando y tactil y envia los toques al motor. Devuelve 0 si se pidio salir (Plus + Minus). */
int input_update(void);

/* Dibuja el cursor del stick derecho (debe llamarse justo antes del swap). */
void input_render_cursor(void);

/* Respuestas a las consultas del motor (se llaman desde jni_fake.c). */
void input_pad_axis(int index);           /* empuja nativeSetPadAxisValues(index, x, y) */
int  input_key_pressed(int android_key);  /* 1 si el boton equivalente esta pulsado */

#endif
