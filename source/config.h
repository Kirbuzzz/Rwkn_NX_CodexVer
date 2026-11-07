/* config.h -- configuracion global.
 *
 * Rayman Adventures (Android arm64, motor UAF de Ubisoft en libuaf.so) en Switch,
 * cargado de forma nativa con so_util. Las rutas se resuelven en tiempo de ejecucion
 * desde la ubicacion del .nro (ver paths.h).
 *
 * Basado en gdash_nx (MIT). Ver LICENSE y CREDITS.md.
 */
#ifndef __CONFIG_H__
#define __CONFIG_H__

/* Reserva para la imagen del .so (libuaf.so ocupa ~40 MB cargada). El resto del heap es newlib. */
#define SO_REGION_MB 128

#define SO_NAME      "libuaf.so"
#define APK_NAME     "Reawaken.apk"
#define CONFIG_NAME  "config.txt"

/* android.os.Build.VERSION.SDK_INT que se informa por el JNI falso */
#define ANDROID_SDK_INT 21

#define ANDROID_PACKAGE "com.ubisoft.raymanadventures"

/* tamano real de la superficie (segun modo acoplado/portatil) */
extern int screen_width;
extern int screen_height;

typedef struct {
  int screen_width;     /* -1 = automatico */
  int screen_height;
  int autoplay;         /* 1 = si todo esta listo, entra al juego sin pulsar A */
  int log_jni;          /* 1 = registra cada llamada JNI en el log (diagnostico) */
  int log_imports;      /* 1 = registra funciones de importacion no resueltas */
} Config;

extern Config config;

int read_config(const char *file);
int write_config(const char *file);

/* "es" para idioma; "es_ES" para el locale que espera UAFJNILib.init */
const char *config_lang_iso2(void);
const char *config_locale(void);

#endif
