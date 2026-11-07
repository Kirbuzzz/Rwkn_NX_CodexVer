# Compatibilidad del Mod Menu original

## Evidencia del APK

`libuaf.so` no declara `libModMenu.so` como dependencia ELF. `libModMenu.so` es un módulo independiente: sus cadenas incluyen `JNI_OnLoad`, `dlopen(targetLibName)`, `dlsym(Java_com_ubisoft_uaf_UAFJNILib_InitGlobal)`, `MSHookFunction` y `A64_HOOK`. También contiene rutas de archivos propias del mod (`hooks.txt`, registros y preferencias). Esto indica que el mod se carga dentro del proceso Android y busca enganchar `InitGlobal`.

El binario original es ARM64 para Android y depende de APIs Android/Bionic y de mecanismos de hook que no existen en Horizon/libnx. No se debe cargar directamente en Switch ni declarar que se cargó cuando no fue así.

## Adaptación actual

`source/modmenu_compat.c` reproduce de forma nativa y explícita la secuencia relevante:

1. `dlopen` virtual: valida que el objetivo sea `libuaf.so`, que el port ya cargó.
2. `dlsym` virtual: valida el símbolo JNI exacto de `InitGlobal` y recibe el puntero resuelto por el cargador del port.
3. Hook nativo: enruta la llamada a través de un wrapper y conserva la llamada original exactamente una vez.
4. Registra el estado de inicialización y cierre.

Esto emula el flujo de integración que usa el mod, no el binario ni todas sus APIs. La interfaz del menú Switch sigue siendo una implementación independiente. Cada función de juego necesita un backend separado y verificado; no se inventan offsets ni se marcan trucos como funcionales sin pruebas.

## Límites conocidos

El adaptador no implementa `MSHookFunction`, KittyMemory, `A64_HOOK`, el cargador ELF Android ni el sistema de scripts `hooks.txt`. Tampoco activa automáticamente las opciones del menú. El siguiente paso es identificar qué funciones del juego y qué datos de guardado usa cada función que se quiera portar.
