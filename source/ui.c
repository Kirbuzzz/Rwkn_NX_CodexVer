/* ui.c -- ver ui.h */
#include <switch.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "error.h"
#include "log.h"
#include "ui.h"
#include "rdl/rdl_apk.h"

#define CANVAS_W 1280
#define CANVAS_H 720

static EGLDisplay s_dpy = EGL_NO_DISPLAY;
static EGLSurface s_surf = EGL_NO_SURFACE;
static EGLContext s_ctx = EGL_NO_CONTEXT;

static bool s_active;
static PadState s_pad;
static int s_w, s_h;

static GLuint s_prog, s_tex;
static GLint s_loc_pos = -1, s_loc_uv = -1, s_loc_tex = -1;
static uint32_t *s_canvas_px;      /* CANVAS_W x CANVAS_H RGBA */
static gfx_image s_bg;             /* fondo ya escalado a CANVAS_W x CANVAS_H (opaco); rgba NULL = sin arte */

/* ------------------------------------------------------------------ EGL */

static void init_egl(void) {
  s_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (s_dpy == EGL_NO_DISPLAY)
    fatal_error("eglGetDisplay fallo.");
  eglInitialize(s_dpy, NULL, NULL);
  eglBindAPI(EGL_OPENGL_ES_API);

  const EGLint cfg_attr[] = {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
    EGL_NONE,
  };
  EGLConfig cfg = NULL;
  EGLint ncfg = 0;
  eglChooseConfig(s_dpy, cfg_attr, &cfg, 1, &ncfg);
  if (ncfg < 1)
    fatal_error("No hay una configuracion EGL utilizable.");

  NWindow *nw = nwindowGetDefault();
  nwindowSetDimensions(nw, s_w, s_h);
  s_surf = eglCreateWindowSurface(s_dpy, cfg, (EGLNativeWindowType)nw, NULL);
  if (s_surf == EGL_NO_SURFACE)
    fatal_error("eglCreateWindowSurface fallo.");

  const EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  s_ctx = eglCreateContext(s_dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
  if (s_ctx == EGL_NO_CONTEXT)
    fatal_error("eglCreateContext fallo.");

  eglMakeCurrent(s_dpy, s_surf, s_surf, s_ctx);
  eglSwapInterval(s_dpy, 1); /* 60 fps */
  glViewport(0, 0, s_w, s_h);
}

EGLDisplay ui_egl_display(void) { return s_dpy; }
EGLSurface ui_egl_surface(void) { return s_surf; }
bool ui_active(void) { return s_active; }

/* ------------------------------------------------------------------- GL */

static GLuint compile(GLenum type, const char *src) {
  GLuint sh = glCreateShader(type);
  glShaderSource(sh, 1, &src, NULL);
  glCompileShader(sh);
  GLint ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char info[256] = { 0 };
    glGetShaderInfoLog(sh, sizeof info, NULL, info);
    LOG("shader de la pantalla no compila: %s", info);
    glDeleteShader(sh);
    return 0;
  }
  return sh;
}

static void init_gl_objects(void) {
  static const char *vs =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() { gl_Position = vec4(a_pos, 0.0, 1.0); v_uv = a_uv; }\n";
  static const char *fs =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main() { gl_FragColor = vec4(texture2D(u_tex, v_uv).rgb, 1.0); }\n";

  GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
  if (!v || !f)
    fatal_error("No se pudieron compilar los shaders de la pantalla.");
  s_prog = glCreateProgram();
  glAttachShader(s_prog, v);
  glAttachShader(s_prog, f);
  glLinkProgram(s_prog);
  GLint ok = 0;
  glGetProgramiv(s_prog, GL_LINK_STATUS, &ok);
  if (!ok)
    fatal_error("No se pudo enlazar el programa de la pantalla.");
  glDeleteShader(v);
  glDeleteShader(f);
  s_loc_pos = glGetAttribLocation(s_prog, "a_pos");
  s_loc_uv  = glGetAttribLocation(s_prog, "a_uv");
  s_loc_tex = glGetUniformLocation(s_prog, "u_tex");

  glGenTextures(1, &s_tex);
  glBindTexture(GL_TEXTURE_2D, s_tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, CANVAS_W, CANVAS_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void ui_init(int w, int h) {
  s_w = w;
  s_h = h;
  init_egl();

  s_canvas_px = malloc((size_t)CANVAS_W * CANVAS_H * sizeof(uint32_t));
  if (!s_canvas_px)
    fatal_error("Sin memoria para la pantalla.");
  gfx_font_init();
  init_gl_objects();

  padConfigureInput(8, HidNpadStyleSet_NpadStandard);
  padInitializeAny(&s_pad);
  s_active = true;
}

void ui_release_stage(void) {
  if (!s_active)
    return;
  s_active = false;
  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  if (s_tex)  glDeleteTextures(1, &s_tex);
  if (s_prog) glDeleteProgram(s_prog);
  s_tex = s_prog = 0;
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  gfx_image_free(&s_bg);
  free(s_canvas_px);
  s_canvas_px = NULL;
  gfx_font_exit();
}

/* ---------------------------------------------------------------- fondo */

void ui_load_background(const char *apk_path) {
  /* arte con el titulo "Reawaken" (xhdpi..xxxhdpi, 1920x1080); respaldo: arte original */
  static const char *const names[] = {
    "res/drawable-xxxhdpi-v4/logorc.png", "res/drawable-xxhdpi-v4/logorc.png",
    "res/drawable-xhdpi-v4/logorc.png",   "res/drawable/logorc.png",
  };
  for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
    void *data = NULL;
    size_t len = 0;
    if (rdl_apk_read(apk_path, names[i], &data, &len) != RDL_OK)
      continue;
    gfx_image raw;
    bool ok = gfx_png_decode(data, len, &raw);
    free(data);
    if (!ok)
      continue;
    ok = gfx_image_cover(&raw, CANVAS_W, CANVAS_H, &s_bg);
    gfx_image_free(&raw);
    if (ok) {
      LOG("fondo: %s", names[i]);
      return;
    }
  }
  LOG("fondo: sin arte (se usa el de respaldo)");
}

/* ------------------------------------------------------------- dibujado */

void ui_present(const scr_model *m) {
  if (!s_active)
    return;
  gfx_canvas c = { s_canvas_px, CANVAS_W, CANVAS_H, CANVAS_W };
  if (s_bg.rgba) gfx_canvas_blit(&c, &s_bg);
  else scr_draw_fallback_background(&c);
  scr_render(&c, m);

  glViewport(0, 0, s_w, s_h);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glUseProgram(s_prog);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, s_tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, CANVAS_W, CANVAS_H, GL_RGBA, GL_UNSIGNED_BYTE, s_canvas_px);
  glUniform1i(s_loc_tex, 0);

  static const GLfloat quad[] = {   /* x, y, u, v  (la fila 0 del lienzo es la de arriba) */
    -1.f,  1.f, 0.f, 0.f,
     1.f,  1.f, 1.f, 0.f,
    -1.f, -1.f, 0.f, 1.f,
     1.f, -1.f, 1.f, 1.f,
  };
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glEnableVertexAttribArray((GLuint)s_loc_pos);
  glEnableVertexAttribArray((GLuint)s_loc_uv);
  glVertexAttribPointer((GLuint)s_loc_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad);
  glVertexAttribPointer((GLuint)s_loc_uv,  2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad + 2);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDisableVertexAttribArray((GLuint)s_loc_pos);
  glDisableVertexAttribArray((GLuint)s_loc_uv);

  eglSwapBuffers(s_dpy, s_surf);
}

/* ---------------------------------------------------------------- mando */

uint64_t ui_poll_buttons(void) {
  padUpdate(&s_pad);
  return padGetButtonsDown(&s_pad);
}

uint64_t ui_wait_input(const scr_model *m, uint64_t mask) {
  mask |= HidNpadButton_Plus;
  while (appletMainLoop()) {
    const uint64_t down = ui_poll_buttons() & mask;
    if (down)
      return down;
    ui_present(m);
  }
  return HidNpadButton_Plus;
}

bool ui_fatal(const char *msg) {
  if (!s_active)
    return false;
  scr_model m;
  memset(&m, 0, sizeof m);
  m.mode = SCR_ERROR;
  snprintf(m.status, sizeof m.status, "Error");
  snprintf(m.detail, sizeof m.detail, "%s", msg);
  snprintf(m.hint, sizeof m.hint, "Detalles en data/log.txt     A / +: salir");
  ui_wait_input(&m, HidNpadButton_A);
  return true;
}
