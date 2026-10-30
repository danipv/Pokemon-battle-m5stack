# Combate por tonos · versión preparada

Entorno PlatformIO: **acoustic-battle**. Conserva los tres iniciales, sus animaciones y el espacio NVS `pokemon-nfc` / clave `cards`. No escribe en las tarjetas. En un robot sin asignaciones, utiliza el mismo asistente de tres tarjetas. No utiliza infrarrojos, Wi-Fi ni microSD.

## Compilar sin modificar ningún robot

```powershell
.venv\Scripts\pio.exe run -e tone-test -e acoustic-battle
```

No se necesita USB. Este comando solo crea los binarios. `tone-test` sigue siendo el entorno predeterminado para no instalar el combate accidentalmente al probar el enlace sencillo.

Cuando se decida probarlo físicamente, instala la misma revisión en ambos robots con `run.bat -Env acoustic-battle -Usb COM3`, cambiando el puerto según corresponda.

## Uso previsto

Cada usuario acerca una tarjeta NFC para elegir un Pokémon. Los robots escuchan y anuncian su elección por turnos. Tras el intercambio de HELLO/ACK comienza el combate automático. Conserva 100 PS, daño determinista, ventajas de tipo y las dos animaciones. Las tarjetas no cambian de Pokémon en mitad de una partida. **Salir / nuevo** cancela y permite elegir otra tarjeta.

Esta primera codificación prioriza notas fáciles de detectar: **cada paquete tarda unos 4,7 segundos**, más el cambio de audio y la pausa de respuesta. El emparejamiento y cada turno pueden tardar varios intercambios. El combate será bastante más lento que una conexión inalámbrica. Solo se contempla una pareja de robots en el mismo entorno acústico.

## Paquetes

Cada mensaje contiene 24 bits del motor (firma, tipo y datos) y un **CRC-8, polinomio 0x07**, calculado sobre esos tres bytes. Conserva los cuatro tipos del motor:

| Tipo | Contenido / función |
|---|---|
| HELLO | Identificador aleatorio de partida y Pokémon local. |
| ACK | Confirma la elección recibida. |
| SYNC | Identificador de sesión y último turno alcanzado. |
| BYE | Cancelación de la partida actual. |

Una cabecera de 2600 Hz abre la trama. Dieciséis símbolos transportan dos bits cada uno, usando 1000, 1400, 1800 y 2200 Hz. Cada símbolo dura 160 ms, con 80 ms de silencio antes del siguiente. Un tono de 3000 Hz cierra la trama. No basta con oír los datos: la longitud, cierre, CRC y firma deben ser válidos.

Los mensajes son distintos de la prueba manual de tres notas; un WAV de Bulbasaur de `tone-test` no puede iniciar una partida.

## Confirmación y reintentos

Ambos robots calculan el mismo orden y daño a partir de los identificadores de la partida. Un turno nuevo exige confirmación del anterior mediante SYNC. Repetir un mensaje no repite el ataque.

El emisor vuelve a escuchar después de transmitir. Antes de emitir se comprueba que no haya una trama en curso ni un tono reciente. Hay pausas aleatorias y un plazo corto de respuesta tras cada recepción, para reducir colisiones. Si se pierde un mensaje se vuelve a anunciar el estado actual. CRC incorrectos y sesiones ajenas se descartan.

Los tiempos del motor son específicos para audio: 60 s sin contacto o 120 s sin avanzar cancelan el enlace. La pantalla muestra pérdida de enlace o error de audio. Al finalizar se repite el estado final durante un periodo acotado, evitando una emisión indefinida tras la partida.

## Aislamiento del audio y bloqueo reportado

Una tarea es propietaria de `M5.Mic` y `M5.Speaker`; ninguna pantalla ni lectura NFC llama directamente a esos dispositivos. Las acciones pasan por una cola acotada y la interfaz consulta instantáneas sin esperar a una grabación. La cola de comandos permite sustituir una solicitud pendiente con **Parar**.

La captura usa bloques de 20 ms y comprueba si pasan más de 400 ms sin completar un bloque. En ese caso publica un error e intenta detener el audio. Una señal de actividad cada 100 ms permite mostrar **Audio sin respuesta** si el controlador deja de avanzar. La interfaz intenta adquirir I2C sin bloquearse mientras el códec cambia de modo. Los pitidos de selección y combate se omiten para no abrir el altavoz mientras el micrófono está activo.

Estas medidas evitan esperas del audio en el bucle de interfaz. Las llamadas internas del controlador siguen dependiendo de M5Unified: no constituyen una garantía de recuperación de todas las averías de hardware. El bloqueo comunicado durante los tonos no se ha reproducido en la versión nueva, porque todavía no se ha instalado para esa prueba.

## Validación realizada

- Compilación de `tone-test` y `acoustic-battle`, sin carga de firmware.
- Las tres secuencias manuales y sus casos de ruido, pausa, nivel bajo y notas ausentes.
- CRC y rechazo de alteraciones de un bit, símbolos modificados y tramas PCM con ruido y desfase respecto a las ventanas del receptor.
- 90 combates simulados con la duración real de las tramas y pérdidas de mensajes.
- Pruebas existentes del motor: 900 combates con pérdidas, duplicados y retrasos.

**Pendiente:** validar los paquetes completos, el cambio de micrófono/altavoz y la respuesta de la pantalla en dos robots reales. La prueba física previa solo confirmó la identificación sencilla de Bulbasaur, no este protocolo de paquetes.
