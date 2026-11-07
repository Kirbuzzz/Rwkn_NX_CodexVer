/* modmenu.c -- interfaz nativa de Reaweaken NX.
 *
 * Es una implementacion nueva para Switch: no carga ni enlaza libModMenu.so,
 * Substrate, KittyMemory ni APIs de Android. Las acciones de juego se dejan
 * deshabilitadas hasta que exista un backend verificado para cada funcion.
 */
#include <switch.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "modmenu.h"
#include "rdl/rdl_gfx.h"

#define MM_W 1280
#define MM_H 720
#define MM_COUNT 7

static const char *const s_items[MM_COUNT] = {
  "Infinite Gems / Food / Tickets",
  "Infinite Incrediball Usage",
  "Infinite Jumps",
  "Complete Current Level",
  "Complete Current Adventure",
  "HD Textures",
  "FPS / Lag Log"
};

static int s_width, s_height, s_selected;
static bool s_ready, s_open, s_block_frame;
static PadState s_pad;
static uint32_t *s_pixels;
static GLuint s_program, s_texture;
static GLint s_pos = -1, s_uv = -1, s_sampler = -1;

static GLuint shader_compile(GLenum type, const char *src) {
  GLuint sh = glCreateShader(type);
  if (!sh) return 0;
  glShaderSource(sh, 1, &src, NULL);
  glCompileShader(sh);
  GLint ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    glDeleteShader(sh);
    return 0;
  }
  return sh;
}

bool modmenu_init(int width, int height) {
  if (s_ready) return true;
  s_width = width > 0 ? width : MM_W;
  s_height = height > 0 ? height : MM_H;
  s_pixels = calloc((size_t)MM_W * MM_H, sizeof(uint32_t));
  if (!s_pixels || !gfx_font_init()) {
    free(s_pixels); s_pixels = NULL;
    LOG("ModMenu: no se pudo reservar el lienzo o cargar la fuente");
    return false;
  }

  static const char *vs_src =
    "attribute vec2 a_pos; attribute vec2 a_uv; varying vec2 v_uv;"
    "void main(){ gl_Position=vec4(a_pos,0.0,1.0); v_uv=a_uv; }";
  static const char *fs_src =
    "precision mediump float; varying vec2 v_uv; uniform sampler2D u_tex;"
    "void main(){ gl_FragColor=texture2D(u_tex,v_uv); }";
  GLuint vs = shader_compile(GL_VERTEX_SHADER, vs_src);
  GLuint fs = shader_compile(GL_FRAGMENT_SHADER, fs_src);
  if (!vs || !fs) {
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    free(s_pixels); s_pixels = NULL;
    gfx_font_exit();
    return false;
  }
  s_program = glCreateProgram();
  glAttachShader(s_program, vs);
  glAttachShader(s_program, fs);
  glLinkProgram(s_program);
  glDeleteShader(vs); glDeleteShader(fs);
  GLint linked = 0;
  glGetProgramiv(s_program, GL_LINK_STATUS, &linked);
  if (!linked) {
    glDeleteProgram(s_program); s_program = 0;
    free(s_pixels); s_pixels = NULL;
    gfx_font_exit();
    return false;
  }
  s_pos = glGetAttribLocation(s_program, "a_pos");
  s_uv = glGetAttribLocation(s_program, "a_uv");
  s_sampler = glGetUniformLocation(s_program, "u_tex");
  glGenTextures(1, &s_texture);
  glBindTexture(GL_TEXTURE_2D, s_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MM_W, MM_H, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  padConfigureInput(8, HidNpadStyleSet_NpadStandard);
  padInitializeAny(&s_pad);
  s_ready = true;
  LOG("ModMenu: interfaz nativa inicializada; acciones de juego pendientes de backend");
  return true;
}

void modmenu_exit(void) {
  if (!s_ready) return;
  glDeleteTextures(1, &s_texture);
  glDeleteProgram(s_program);
  s_texture = s_program = 0;
  free(s_pixels); s_pixels = NULL;
  gfx_font_exit();
  s_ready = false;
  s_open = false;
}

bool modmenu_is_open(void) { return s_open; }

bool modmenu_update(void) {
  if (!s_ready) return false;
  s_block_frame = false;
  padUpdate(&s_pad);
  const u64 down = padGetButtonsDown(&s_pad);
  const u64 held = padGetButtons(&s_pad);
  if ((held & HidNpadButton_L) && (held & HidNpadButton_R) && (down & HidNpadButton_Down)) {
    s_open = !s_open;
    s_block_frame = true;
    LOG("ModMenu: %s", s_open ? "abierto" : "cerrado");
  } else if (s_open) {
    if (down & (HidNpadButton_B | HidNpadButton_Up | HidNpadButton_Down | HidNpadButton_A)) s_block_frame = true;
    if (down & HidNpadButton_B) s_open = false;
    if (down & HidNpadButton_Up) s_selected = (s_selected + MM_COUNT - 1) % MM_COUNT;
    if (down & HidNpadButton_Down) s_selected = (s_selected + 1) % MM_COUNT;
    if (down & HidNpadButton_A)
      LOG("ModMenu: opcion '%s' seleccionada; backend aun no implementado", s_items[s_selected]);
  }
  return s_open || s_block_frame;
}

static void draw_panel(void) {
  gfx_canvas c = { s_pixels, MM_W, MM_H, MM_W };
  memset(s_pixels, 0, (size_t)MM_W * MM_H * sizeof(uint32_t));

  /* Fondo y panel. */
  gfx_round_rect(&c, 290, 72, 700, 576, 18, GFX_RGBA(17, 22, 34, 255));
  gfx_fill_rect(&c, 310, 92, 660, 5, GFX_RGBA(76, 170, 255, 255));
  gfx_text(&c, GFX_FONT_LARGE, 330, 145, GFX_RGBA(255,255,255,255), "REAWAKEN MOD MENU");
  gfx_text(&c, GFX_FONT_SMALL, 332, 174, GFX_RGBA(167,184,207,255), "Switch native edition  |  build independiente");

  const int x = 322, y0 = 202, row_h = 48, row_gap = 7;
  for (int i = 0; i < MM_COUNT; i++) {
    const int y = y0 + i * (row_h + row_gap);
    const bool selected = (i == s_selected);
    gfx_round_rect(&c, x, y, 636, row_h, 8,
      selected ? GFX_RGBA(42, 76, 112, 255) : GFX_RGBA(29, 37, 52, 255));
    gfx_text(&c, GFX_FONT_MEDIUM, x + 18, y + 30,
      selected ? GFX_RGBA(255,255,255,255) : GFX_RGBA(211,220,232,255), s_items[i]);
    gfx_text_right(&c, GFX_FONT_SMALL, x + 615, y + 29,
      GFX_RGBA(255,190,105,255), "PENDIENTE");
  }

  gfx_text(&c, GFX_FONT_SMALL, 330, 596, GFX_RGBA(255,205,130,255),
    "Las funciones necesitan adaptadores verificados para libuaf.so.");
  gfx_text(&c, GFX_FONT_SMALL, 330, 624, GFX_RGBA(180,195,215,255),
    "L + R + Abajo: abrir/cerrar     A: seleccionar     B: cerrar");
}

typedef struct {
  GLint enabled, size, type, normalized, stride, buffer;
  void *pointer;
} AttribState;

static void save_attrib(GLuint index, AttribState *out) {
  glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &out->enabled);
  glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_SIZE, &out->size);
  glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_TYPE, &out->type);
  glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &out->normalized);
  glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &out->stride);
  glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &out->buffer);
  glGetVertexAttribPointerv(index, GL_VERTEX_ATTRIB_ARRAY_POINTER, &out->pointer);
}

static void restore_attrib(GLuint index, const AttribState *state) {
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)state->buffer);
  glVertexAttribPointer(index, state->size, (GLenum)state->type,
                        (GLboolean)state->normalized, state->stride, state->pointer);
  if (state->enabled) glEnableVertexAttribArray(index);
  else glDisableVertexAttribArray(index);
}

void modmenu_render(void) {
  if (!s_ready || !s_open) return;
  GLint old_program = 0, old_viewport[4] = {0}, old_active_texture = GL_TEXTURE0;
  GLint old_texture0 = 0, old_array_buffer = 0;
  GLint old_blend_src = GL_ONE, old_blend_dst = GL_ZERO;
  const GLboolean blend_was_enabled = glIsEnabled(GL_BLEND);
  const GLboolean depth_was_enabled = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean cull_was_enabled = glIsEnabled(GL_CULL_FACE);
  glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
  glGetIntegerv(GL_VIEWPORT, old_viewport);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active_texture);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture0);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_array_buffer);
  glGetIntegerv(GL_BLEND_SRC_RGB, &old_blend_src);
  glGetIntegerv(GL_BLEND_DST_RGB, &old_blend_dst);
  AttribState old_pos = {0}, old_uv = {0};
  if (s_pos >= 0) save_attrib((GLuint)s_pos, &old_pos);
  if (s_uv >= 0) save_attrib((GLuint)s_uv, &old_uv);

  draw_panel();
  glBindTexture(GL_TEXTURE_2D, s_texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MM_W, MM_H, GL_RGBA,
                  GL_UNSIGNED_BYTE, s_pixels);
  glViewport(0, 0, s_width, s_height);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(s_program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, s_texture);
  glUniform1i(s_sampler, 0);
  static const GLfloat quad[] = {
    -1.f, 1.f, 0.f, 0.f,  1.f, 1.f, 1.f, 0.f,
    -1.f,-1.f, 0.f, 1.f,  1.f,-1.f, 1.f, 1.f
  };
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glEnableVertexAttribArray((GLuint)s_pos);
  glEnableVertexAttribArray((GLuint)s_uv);
  glVertexAttribPointer((GLuint)s_pos, 2, GL_FLOAT, GL_FALSE, 4*sizeof(GLfloat), quad);
  glVertexAttribPointer((GLuint)s_uv, 2, GL_FLOAT, GL_FALSE, 4*sizeof(GLfloat), quad+2);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  if (s_pos >= 0) restore_attrib((GLuint)s_pos, &old_pos);
  if (s_uv >= 0) restore_attrib((GLuint)s_uv, &old_uv);
  if (blend_was_enabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
  glBlendFunc((GLenum)old_blend_src, (GLenum)old_blend_dst);
  if (depth_was_enabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
  if (cull_was_enabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)old_array_buffer);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture0);
  glActiveTexture((GLenum)old_active_texture);
  glUseProgram((GLuint)old_program);
  glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
}
