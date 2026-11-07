# Creditos

- **gdash_nx** (port de Geometry Dash a Switch): de aqui salen casi todos los archivos base del cargador
  (`so_util`, `pthr`, `libc_shim`, `net_shim`, `util`, `prefs`, y el modelo de objetos de `jni_fake`).
- **TheOfficialFloW**: cargador de `.so` de Android original (so-loader para PS Vita).
- **fgsfds** (max_nx): base del cargador para Switch.
- **Proyecto Reawaken**: revivio Rayman Adventures; el servidor del obb y el arte "REAWAKEN" son suyos.
- **Ubisoft**: Rayman Adventures. Este repositorio NO incluye `libuaf.so`, el apk, los assets ni el obb: cada
  usuario usa su propio `Reawaken.apk`.
- **DejaVu / Bitstream Vera**: fuente DejaVu Sans Bold (rasterizada en `source/rdl/rdl_font_data.c`).
- **Mozilla CA bundle** (via certifi, MPL-2.0): `source/rdl/rdl_cacert.c`.

Lo escrito de cero para este proyecto: descargador (`source/rdl/`), pantalla de arranque (`ui.c`, `launcher.c`),
emulacion de OpenSL ES (`opensl*.c`), de AAssetManager (`aasset.c`), JNI de UAF (`jni_fake.c`), entrada (`input.c`),
arranque del motor (`game.c`).
