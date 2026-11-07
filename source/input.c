/* input.c -- ver input.h */
#include <math.h>
#include <string.h>
#include <switch.h>
#include <GLES2/gl2.h>

#include "config.h"
#include "input.h"
#include "jni_fake.h"
#include "log.h"

#define MAX_FINGERS 10

typedef struct { int active; float x, y; } Finger;

static InputSink s_sink;
static PadState s_pad;
static int s_w = 1280, s_h = 720;
static Finger s_cur[MAX_FINGERS + 1];        /* ultimo estado enviado; el indice MAX_FINGERS es el cursor virtual */
static float s_cursor_x, s_cursor_y;
static u64 s_cursor_tick;
static int s_connected_sent = -1;
static int s_game_input_blocked;

#define VPTR_CURSOR MAX_FINGERS

void input_set_game_input_blocked(int blocked) { s_game_input_blocked = blocked != 0; }

void input_init(int w, int h, const InputSink *sink) {
  s_w = w;
  s_h = h;
  s_sink = *sink;
  padConfigureInput(8, HidNpadStyleSet_NpadStandard);
  padInitializeAny(&s_pad);
  hidInitializeTouchScreen();
  s_cursor_x = (float)w / 2.0f;
  s_cursor_y = (float)h / 2.0f;
  memset(s_cur, 0, sizeof s_cur);
}

/* ------------------------------------------------------------- toques */

static void send_touch(int id, int action, float px, float py, float x, float y) {
  if (s_sink.touch)
    s_sink.touch(fake_env, NULL, px, py, x, y, 0, id, action);
}

/* compara el estado deseado con el enviado y emite abajo/mover/arriba */
static void commit_pointers(Finger *want) {
  for (int i = 0; i <= MAX_FINGERS; i++) {
    Finger *c = &s_cur[i], *n = &want[i];
    if (n->active && !c->active) {
      send_touch(i, 0, n->x, n->y, n->x, n->y);
    } else if (n->active && c->active) {
      if (n->x != c->x || n->y != c->y)
        send_touch(i, 2, c->x, c->y, n->x, n->y);
    } else if (!n->active && c->active) {
      send_touch(i, 1, c->x, c->y, c->x, c->y);
    }
    *c = *n;
  }
}

/* ---------------------------------------------------------- cursor / mando */

static float stick_axis(int stick, int axis) {
  const HidAnalogStickState st = padGetStickPos(&s_pad, stick);
  const float v = (float)(axis ? st.y : st.x) / 32767.0f;
  return v;
}

static void cursor_update(void) {
  float dx = stick_axis(1, 0), dy = stick_axis(1, 1);     /* stick derecho */
  if (fabsf(dx) < 0.25f) dx = 0.f;
  if (fabsf(dy) < 0.25f) dy = 0.f;
  if (dx == 0.f && dy == 0.f)
    return;
  const float speed = 900.0f * ((float)s_h / 720.0f);
  s_cursor_x += dx * speed / 60.0f;
  s_cursor_y -= dy * speed / 60.0f;                       /* stick arriba = cursor arriba */
  if (s_cursor_x < 0.f) s_cursor_x = 0.f;
  if (s_cursor_y < 0.f) s_cursor_y = 0.f;
  if (s_cursor_x > (float)s_w - 1.f) s_cursor_x = (float)s_w - 1.f;
  if (s_cursor_y > (float)s_h - 1.f) s_cursor_y = (float)s_h - 1.f;
  s_cursor_tick = armGetSystemTick();
}

static int cursor_visible(void) {
  return s_cursor_tick && (armGetSystemTick() - s_cursor_tick) < armGetSystemTickFreq() * 3;
}

void input_render_cursor(void) {
  if (!cursor_visible())
    return;
  const GLboolean had_scissor = glIsEnabled(GL_SCISSOR_TEST);
  GLint old_box[4];
  GLfloat old_clear[4];
  glGetIntegerv(GL_SCISSOR_BOX, old_box);
  glGetFloatv(GL_COLOR_CLEAR_VALUE, old_clear);
  glEnable(GL_SCISSOR_TEST);

  const int cx = (int)s_cursor_x, cy = s_h - 1 - (int)s_cursor_y;   /* GL: origen abajo a la izquierda */
  const int sz = s_h / 90;
  glScissor(cx - sz / 2 - 1, cy - sz / 2 - 1, sz + 2, sz + 2);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  glScissor(cx - sz / 2, cy - sz / 2, sz, sz);
  glClearColor(1.f, 1.f, 1.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glScissor(old_box[0], old_box[1], old_box[2], old_box[3]);
  glClearColor(old_clear[0], old_clear[1], old_clear[2], old_clear[3]);
  if (!had_scissor)
    glDisable(GL_SCISSOR_TEST);
}

/* ---- consultas que hace el motor (via JNI) ---- */

void input_pad_axis(int index) {
  if (s_game_input_blocked) return;
  /* Android: Y positivo hacia abajo; libnx: hacia arriba. Stick izquierdo; la cruceta tiene prioridad si se pulsa. */
  float x = stick_axis(0, 0), y = -stick_axis(0, 1);
  const u64 b = padGetButtons(&s_pad);
  if (b & (HidNpadButton_Left | HidNpadButton_Right | HidNpadButton_Up | HidNpadButton_Down)) {
    x = (b & HidNpadButton_Right ? 1.f : 0.f) - (b & HidNpadButton_Left ? 1.f : 0.f);
    y = (b & HidNpadButton_Down ? 1.f : 0.f) - (b & HidNpadButton_Up ? 1.f : 0.f);
  }
  if (fabsf(x) < 0.15f) x = 0.f;
  if (fabsf(y) < 0.15f) y = 0.f;
  if (s_sink.pad_axis)
    s_sink.pad_axis(fake_env, NULL, index, x, y);
}

/* Codigos de tecla de Android -> botones de la Switch, por POSICION (A de Xbox = abajo = B de Nintendo) */
int input_key_pressed(int key) {
  if (s_game_input_blocked) return 0;
  const u64 b = padGetButtons(&s_pad);
  switch (key) {
    case 96:  return !!(b & HidNpadButton_B);        /* BUTTON_A  (abajo)    */
    case 97:  return !!(b & HidNpadButton_A);        /* BUTTON_B  (derecha)  */
    case 99:  return !!(b & HidNpadButton_Y);        /* BUTTON_X  (izquierda)*/
    case 100: return !!(b & HidNpadButton_X);        /* BUTTON_Y  (arriba)   */
    case 102: return !!(b & HidNpadButton_L);        /* L1 */
    case 103: return !!(b & HidNpadButton_R);        /* R1 */
    case 104: return !!(b & HidNpadButton_ZL);       /* L2 */
    case 105: return !!(b & HidNpadButton_ZR);       /* R2 */
    case 106: return !!(b & HidNpadButton_StickL);
    case 107: return !!(b & HidNpadButton_StickR);
    case 108: return !!(b & HidNpadButton_Plus);     /* START  */
    case 109: return !!(b & HidNpadButton_Minus);    /* SELECT */
    case 19:  return !!(b & HidNpadButton_Up);
    case 20:  return !!(b & HidNpadButton_Down);
    case 21:  return !!(b & HidNpadButton_Left);
    case 22:  return !!(b & HidNpadButton_Right);
    default:  return 0;
  }
}

/* ------------------------------------------------------------ por frame */

int input_update(void) {
  padUpdate(&s_pad);
  const u64 down = padGetButtonsDown(&s_pad), held = padGetButtons(&s_pad);

  /* mando conectado (el motor tambien lo consulta por JNI) */
  const int connected = padIsConnected(&s_pad) ? 1 : 0;
  g_gamepad_connected = connected;
  if (connected != s_connected_sent) {
    s_connected_sent = connected;
    if (s_sink.pad_connected)
      s_sink.pad_connected(fake_env, NULL, connected);
  }

  /* salir: Plus + Minus a la vez */
  if ((held & HidNpadButton_Plus) && (held & HidNpadButton_Minus))
    return 0;
  if (s_game_input_blocked) {
    Finger none[MAX_FINGERS + 1];
    memset(none, 0, sizeof none);
    commit_pointers(none);
    return 1;
  }
  if ((down & HidNpadButton_Minus) && !(held & HidNpadButton_Plus) && s_sink.back_pressed)
    s_sink.back_pressed(fake_env, NULL);

  cursor_update();

  Finger want[MAX_FINGERS + 1];
  memset(want, 0, sizeof want);

  HidTouchScreenState ts = { 0 };
  if (hidGetTouchScreenStates(&ts, 1)) {
    const float sx = (float)s_w / 1280.0f, sy = (float)s_h / 720.0f;
    for (int i = 0; i < (int)ts.count; i++) {
      const int id = (int)ts.touches[i].finger_id;
      if (id < 0 || id >= MAX_FINGERS) continue;
      want[id].active = 1;
      want[id].x = (float)ts.touches[i].x * sx;
      want[id].y = (float)ts.touches[i].y * sy;
    }
  }
  if (held & HidNpadButton_StickR) {            /* R3: toque en el cursor */
    want[VPTR_CURSOR].active = 1;
    want[VPTR_CURSOR].x = s_cursor_x;
    want[VPTR_CURSOR].y = s_cursor_y;
  }
  commit_pointers(want);
  return 1;
}
