# Primera prueba en consola

Objetivo: no que funcione a la primera, sino **saber exactamente donde se rompe**.

## Pasos

1. Copia `Reaweaken.nro` y tu `Reawaken.apk` a `sdmc:/switch/Reaweaken/`.
2. Abre el `.nro` **manteniendo `R`** sobre un juego (game override).
3. Deberias ver la pantalla con el arte de Reawaken: "Preparando archivos..." y luego "Conectando...".
4. Descarga el obb (A = normal) y, cuando diga "Todo listo para jugar", pulsa **A**.
5. Pasa lo que pase, **copia `sdmc:/switch/Reaweaken/data/log.txt`** y mandalo.

## Donde puede romperse y que dice el log

| Ultima linea del log | Significa |
|---|---|
| `Reaweaken NX (argv0=...` y nada mas | fallo al iniciar EGL/pantalla (mesa/nouveau) |
| `manifest: ...` + `Detalle: curl NN: ...` | red/TLS (certificado, DNS); el numero de curl lo dice |
| `lanzador: jugar` pero no `cargando libuaf.so` | fallo antes del cargador (heap, ui_release_stage) |
| `cargando libuaf.so ...` y se cuelga | `so_load`/relocacion (memoria de codigo no disponible: falta `R`) |
| `init_array ejecutado` ausente | crash en un constructor estatico de libuaf (OpenSSL, std::) |
| `JNI_OnLoad ok` ausente | JNI_OnLoad pidio algo al JNI falso |
| `InitGlobal ok` ... `initNativeEngine ok` | la secuencia de arranque; la ultima que aparece es la que falla |
| `JNI sin implementar (...)` | llamadas de Java que el motor hace y no estan modeladas: **lista exactamente que falta** |
| `frame 1`, `frame 60`... | el motor corre; si la pantalla sale negra: texturas (ver abajo) |

Si el sistema muestra un error de "crash" de Horizon (codigo 2168-xxxx), apunta tambien la direccion que da.

## Riesgos conocidos

- **Texturas comprimidas**: el motor elige formato por las extensiones GL (PVRTC / ATC / S3TC). mesa en Switch ofrece
  S3TC; si el obb no trae esa variante, saldran negras o fallara `glCompressedTexImage2D`.
- **Videos de intro**: `playMovie` devuelve 0 (omitido); si el motor espera su final, se quedara ahi.
- **Teclado en pantalla**, GDPR, compras, anuncios, Google Play: desactivados/simulados.
- **Audio**: Wwise sobre OpenSL emulado; la salida abre SDL2 al primer `SetPlayState(PLAYING)`.
