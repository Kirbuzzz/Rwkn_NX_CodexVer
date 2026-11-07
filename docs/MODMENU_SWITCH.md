# Mod Menu nativo para Switch

Esta implementación crea una interfaz propia para Switch. No carga `libModMenu.so` ni reutiliza código binario del APK. La biblioteca original declara dependencias Android (`libandroid.so`, `libEGL.so`, `libGLESv2.so`, `libdl.so`, entre otras) y contiene referencias a `MSHookFunction`, `KittyMemory` y `A64_HOOK`; esas APIs no están disponibles tal cual en libnx.

## Controles

- `L + R + Abajo`: abrir/cerrar.
- `Arriba` / `Abajo`: cambiar la opción seleccionada.
- `A`: seleccionar (por ahora registra la selección; no ejecuta el truco).
- `B`: cerrar.
- `Plus + Minus`: salir del homebrew.

## Opciones identificadas en el APK

1. Infinite Gems / Food / Tickets.
2. Infinite Incrediball Usage.
3. Infinite Jumps.
4. Complete Current Level.
5. Complete Current Adventure.
6. HD Textures.
7. FPS / Lag Log (diagnóstico identificado en cadenas de la biblioteca).

## Estado real

La interfaz, navegación con mando, bloqueo de entrada al juego mientras está abierta y composición sobre el frame se implementan en `source/modmenu.c`, `source/input.c` y `source/game.c`.

**Las acciones de juego están pendientes.** No se han identificado ni validado los campos, funciones o estructuras internas de `libuaf.so` que controlan gemas, comida, tickets, Incrediballs, saltos o progreso. Por eso las opciones se muestran como `PENDIENTE`; pulsar A no modifica partidas ni memoria. No se deben inventar offsets ni aplicar parches de memoria sin validar la versión exacta del motor y sus efectos sobre los guardados.

`HD Textures` además necesita los datos HD compatibles del juego; no basta con activar una casilla. El APK contiene la biblioteca original, pero el port no la enlaza ni la distribuye dentro del NRO.

## Dependencias de la interfaz

No añade dependencias externas nuevas. Usa las bibliotecas ya requeridas por el proyecto: libnx, EGL/GLES2 y el renderer de software `rdl_gfx` (fuente y fuente tipográfica embebida del propio proyecto). La interfaz se inicializa después de arrancar el motor, comparte el contexto EGL existente y se dibuja antes del `eglSwapBuffers` del frame.
