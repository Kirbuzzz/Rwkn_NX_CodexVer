# Diagnostico del APK y compatibilidad NX

## Bibliotecas nativas encontradas en Reawaken.apk (arm64-v8a)

| Biblioteca | Funcion probable / dependencias declaradas |
|---|---|
| `libuaf.so` | Motor UAF del juego. Depende de GLESv2, GLESv1_CM, OpenSL ES, Android, log, z, C/C++ y dl. |
| `libModMenu.so` | Mod menu Android independiente. Depende de `libandroid.so`, EGL y GLESv2, además de libc/libm/libdl. |
| `libEncryptorP.so` | Componente auxiliar; depende de log, libc, libm, libstdc++ y libdl. |
| `libnms.so` | Componente auxiliar de SDK; depende de log, libc, libstdc++, libm y libdl. |
| `libapminsighta.so` | Componente de analitica/monitorizacion; depende de bibliotecas Android basicas. |
| `libapminsightb.so` | Componente de analitica/monitorizacion; depende de bibliotecas Android basicas. |

El port actual carga directamente `libuaf.so`; no implementa un runtime Android completo ni ejecuta
la actividad Java del APK. Por eso, copiar `libModMenu.so` a la SD no lo integra automaticamente.
Su `JNI_OnLoad` y sus dependencias Android tendrian que adaptarse por separado.

## Cambios de compatibilidad incluidos

- El sample rate consultado mediante `AudioTrack.getNativeOutputSampleRate` ahora devuelve 48000 Hz,
  consistente con el mezclador OpenSL/SDL del port.
- Se agregan respuestas neutrales para llamadas del SDK movil observadas en el log:
  `getClassLoader`, `loadClass`, `getFilesDir`, `getPath`, `getDeviceGameVersion`,
  `keyChainGetDataForKey`, `getFilePath`, `getURLParams`, `sync`, `SetGameActivity`,
  `PrepareFile`, `AutoConnect`, `SetUpdateTimeIntervalNative` y `OnMsdkPause`.
- `FindClass` reutiliza objetos de clase y registra cada nombre una sola vez, evitando asignaciones
  y lineas de log repetidas cuando el motor consulta `AudioTrack` en cada frame.
- Se conserva `screen_width`/`screen_height` como definiciones unicas en `config.c`.
- El enlace usa el driver C++ (`$(CXX)`) para resolver simbolos de runtime C++ requeridos por el `.so`.

## Limite de esta revision

El log de consola confirma que `libuaf.so` se carga, `JNI_OnLoad` y la secuencia de inicializacion
terminan, y el bucle llega al menos a `frame 600`. Eso significa que el proceso no esta detenido
durante el arranque; aun falta diagnosticar la pantalla que queda mostrando el indicador de carga.
Los cambios anteriores son compatibilidad razonable con las llamadas observadas, no una garantia
de que el juego ya llegue al menu.

Las pruebas OpenSL ES de escritorio pasan (80 comprobaciones). No se pudo ejecutar una compilacion
NRO en este entorno porque no hay toolchain devkitPro/libnx instalado.
