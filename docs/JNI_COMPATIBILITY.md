# Compatibilidad JNI de Android

Esta capa no ejecuta Java/ART. Reproduce en Switch las operaciones de Android que el motor consulta durante el arranque, y deja registradas las que todavia necesitan una implementacion especifica.

## Operaciones adaptadas

- `getFilesDir()` / `File.getPath()`: usan `path_data()` como directorio escribible.
- `getExternalStoragePath()`: devuelve `path_data()` con separador final.
- `getDeviceGameVersion()`: devuelve `3.9.95`, version declarada por el APK.
- `AudioTrack.getNativeOutputSampleRate()`: devuelve 48000 Hz.
- `SetGameActivity()`: conserva el objeto de actividad falso para consultas posteriores del SDK.
- `PrepareFile(src, dst)`: replica la copia que hace `ubisoft.mobile.mobileSDK.Utils` en el APK: lee `assets/<src>` y lo copia a `dst`. El destino Android privado se traduce a la carpeta `data/` de Switch; se crean directorios, se escribe primero a un temporal y se renombra al finalizar. Se rechazan rutas con `..` y destinos fuera de `data/`.
- `ClassLoader.loadClass()`: devuelve un objeto de clase simulado; no carga bytecode Java.

## Como interpretar la siguiente prueba

Busca en `data/log.txt` una linea `JNI PrepareFile:`:

- `copiado ...`: el recurso se encontro y se copio.
- `no existe el asset ...`: el nombre solicitado no esta en el directorio `assets/` extraido; revisar el argumento `src`.
- `destino fuera de data/`: el APK solicita escribir fuera del area de datos permitida; revisar el argumento `dst` antes de ampliar la traduccion.
- `error copiando` / `no se pudo finalizar`: problema de escritura o del sistema de archivos.

Si `PrepareFile` termina correctamente y el juego sigue en la pantalla de carga, el siguiente objetivo sera identificar si el motor espera una respuesta de servicios/conectividad o algun recurso del OBB. No se fuerza `isConnected()` a verdadero ni se simula una autenticacion exitosa sin evidencia.
