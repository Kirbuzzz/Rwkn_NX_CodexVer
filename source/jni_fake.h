/* jni_fake.h -- JNIEnv/JavaVM falsos para libuaf.so
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __JNI_FAKE_H__
#define __JNI_FAKE_H__

#include <stdint.h>

// The fake JNIEnv / JavaVM handed to the game. Both are pointers to a pointer
// to a function table, matching the real JNI ABI the game calls through.
extern void *fake_env;
extern void *fake_vm;

void jni_init(void);

// helpers for building the Java objects the natives expect
void *jni_make_object(const char *label);
void *jni_make_string(const char *utf);
void *jni_make_string_array(int n, const char **strs);
void *jni_make_int_array(int n, const int *vals);
void *jni_make_float_array(int n, const float *vals);
void *jni_make_byte_array(int n, const void *data);
void jni_prim_array_fill(void *arr, const void *data, int n, int elem);
const char *jni_string_utf(void *jstr);

// el motor pidio salir (GameActivity.exitTheGame / moveGameToBackGround)
extern volatile int g_quit_requested;
// lo mantiene el modulo de entrada: GameActivity.isGamePadConnected
extern volatile int g_gamepad_connected;

#endif
