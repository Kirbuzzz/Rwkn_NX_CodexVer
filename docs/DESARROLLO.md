# Notas de desarrollo

## Secuencia de arranque que replica `game.c`

Sacada del desensamblado del apk (`tools/dexdis.py classes4.dex 'Lcom/ubisoft/uaf/GameActivity;' initEngine`, etc.):

| Origen en Java | Llamada nativa (UAFJNILib) | Valor que le damos |
|---|---|---|
| `GameActivity.initEngine` | `InitGlobal(Object)` | objeto falso `com/ubisoft/uaf/GameActivity` |
| | `InitMobileSDK()` | |
| | `callGenericNativeMethod("FPSMANAGER_LOG_TO_FILE" / "setDeviceModel" / "setDeviceCodeName" / "setOSVersion" / "setCodeVersion" / "setMultiAPKVersion" / "setConnectionStatus" / "setPlayLicensing" / "setEngineVersion" / "setDataVersion", int, float, String)` | ver `game_boot()` |
| | `setupDeviceInfo(modelo, codigo, modelo_min, codigo_min, AssetManager)` | "Nintendo Switch", "nx", ... |
| `GL2JNIView.init` | `callGenericNativeMethod("setAppRootPath", ..., ruta)` | `<base>/data` |
| `Renderer.onSurfaceCreated` | `initNativeEngine(apk, raiz, obb, AssetManager)` | rutas reales de la SD |
| | `init(w, h, "es_ES", dpi)` | 1280x720 / 1920x1080, dpi 320 |
| | `setNativeScreenResolution(w, h)`, `setDeviceOrientation(w, h)`, `setDeviceTouchOrientation(w, h)` | |
| | `callGenericNativeMethod("setAppInstaller", ...)` | `""` |
| `Renderer.onDrawFrame` | `step()` | una vez por frame |

Constantes del juego (`res/raw/game.properties` del apk): `engine.version=277216`, `data.version=127897`,
`package.main.filename=main.2086.com.ubisoft.raymanadventures.obb`.

Entrada:
- Tactil: `touch(prevX, prevY, x, y, 0, puntero, accion)`; accion 0 abajo, 1 arriba, 2 mover, 3 cancelar.
- Mando: el motor **consulta** a Java (`isGamePadConnected`, `getGamePadAxisValues(idx)` -> Java responde llamando a
  `nativeSetPadAxisValues(idx, x, y)`, `isGamePadKeyPressed(idx, codigoAndroid)`).

## Anadir una llamada JNI

Mira el log (`log_jni=1`): cada llamada desconocida sale una vez como
`JNI sin implementar (tipo): nombre firma -> valor por defecto`. En `jni_fake.c` busca la funcion segun el tipo de
retorno (`call_boolean`, `call_int`, `call_object`, `call_void`...) y anade un `if (!strcmp(name, "..."))`.
Los argumentos se leen con `va_arg(va, int)` / `va_arg(va, void *)` (floats y doubles llegan como `double`).
La lista de metodos que el motor puede llamar esta en `docs/jni_metodos_java.txt`; las nativas, en
`docs/uafjnilib_nativas.txt`.

## Herramientas (`tools/`)

- `dexclass.py f.dex 'Lcom/x/Clase;'` lista metodos (`native`, `static`, firma).
- `dexdis.py f.dex 'Lcom/x/Clase;' metodo` desensamblado legible (invoke, const-string, sget...).
- `dexcalls.py f.dex nombre...` desde donde se llama a una clase y con que constantes.
- `dexstr.py f.dex` tabla de cadenas.
- `gen_font.py`, `gen_cacert.py` regeneran `rdl_font_data.c` y `rdl_cacert.c`.

## Pendiente / hipotesis sin comprobar

- `setupDeviceInfo`, `setOSVersion`, dpi=320 y `setAppInstaller=""` son suposiciones razonables, no valores verificados.
- `addAppSignature` no se llama (Java lo hace con la firma del apk); si el motor valida firmas, saldra en el log.
- `playMovie` -> 0 (se omiten videos). `VKeyboardOpen` no hace nada (sin teclado en pantalla todavia).
- GDPR/COPPA: se marca "ya mostrado" para no abrir dialogos de consentimiento.
- Audio: SDL2 en modo "push" a 48 kHz estereo; el mezclador corre en un hilo con TLS falso (`pthr_ensure_fake_tls`).
- El formato de textura que elige el motor depende de las extensiones de mesa (ver `PRIMERA_PRUEBA.md`).
