# Pokémon Battle M5Stack · prueba por tonos v0.1

Primera prueba de comunicación por **altavoz y micrófono** entre dos StackChan K151/CoreS3. Es el programa predeterminado del repositorio. De momento identifica Bulbasaur, Charmander y Squirtle mediante secuencias cortas; todavía no transmite turnos ni inicia combates.

## Prueba rápida

1. Instala esta versión en ambos robots y colócalos inicialmente a unos 20–40 cm, en un lugar tranquilo.
2. En el receptor, pulsa **Escuchar**. Aparece el nivel de entrada y el tono detectado.
3. En el emisor, elige un Pokémon y pulsa **Emitir tonos**.
4. El receptor debe mostrar **RX: Bulbasaur**, **RX: Charmander** o **RX: Squirtle**, y aumentar el contador.
5. Repite con los otros Pokémon e intercambia emisor/receptor para comprobar ambos sentidos.

**Parar** detiene la prueba. El micrófono y el altavoz comparten I2S: un robot escucha o emite, nunca ambas cosas a la vez. No pulses Emitir simultáneamente en ambos.

No necesita Wi-Fi ni microSD. Esta prueba no lee ni modifica las tarjetas: **las asignaciones NFC anteriores permanecen guardadas**. No hagas un borrado completo de flash. La cabeza se centra al arrancar.

## Instalación en Windows

Con Python y Git instalados, clona el repositorio y prepara el entorno una vez:

```powershell
git clone https://github.com/danipv/Pokemon-battle-m5stack.git
cd Pokemon-battle-m5stack
.\setup.bat
.\run.bat -Usb COM3
```

Para actualizar una instalación existente:

```powershell
git pull
.\run.bat -Usb COM3
```

Cambia COM3 por tu puerto. El entorno predeterminado es **tone-test**. Para ver el registro sin cargar otra vez: `run.bat -NoFlash -Monitor -Usb COM3`.

Con PlatformIO directamente: `pio run -e tone-test -t upload` y `pio device monitor -e tone-test -b 115200`.

## Prueba con un ordenador o móvil

En `assets/tones/` están los tres WAV de referencia. Pon el robot en **Escuchar** y reproduce uno cerca de él, a volumen moderado. Cada archivo tiene medio segundo de silencio al principio y al final. Para regenerarlos, sin dependencias adicionales: `python tools/make_tone_wavs.py assets/tones`.

## Secuencias

Cada nota dura 300 ms, con 100 ms de silencio entre notas:

| Pokémon | Inicio | Identificador | Final |
|---|---|---|---|
| Bulbasaur | 2400 Hz | 1000 Hz | 2800 Hz |
| Charmander | 2400 Hz | 1400 Hz | 2800 Hz |
| Squirtle | 2400 Hz | 1800 Hz | 2800 Hz |

El receptor muestrea a 16 kHz en bloques de 20 ms y mide las cinco frecuencias con Goertzel. Exige un tono dominante durante al menos 60 ms y la secuencia inicio → identificador → final en menos de dos segundos. Ignora tonos aislados, demasiado cortos o muy débiles. Un tono sostenido no genera recepciones duplicadas.

Esta versión no incluye CRC, reintentos ni emparejamiento: sirve para validar el enlace acústico antes de conectarlo al combate. El ruido, la distancia y el volumen deben probarse físicamente.

## Código y pruebas

- `src/tone_test.cpp`: pantalla y cambio seguro entre altavoz y micrófono.
- `include/tone_protocol.h`: detector y secuencia compartidos con las pruebas.
- `tests/tone_test.cpp`: tres secuencias con ruido y offset de continua; notas ausentes, cortas y débiles; tiempos de espera y duplicados.
- `assets/tones/`: WAV de referencia.

```bash
g++ -std=c++17 -Wall -Wextra -Werror -I include tests/tone_test.cpp -o tone_test
./tone_test
```

El monitor USB acepta `listen`, `send 0`, `send 1`, `send 2` y `stop`. Muestra nivel, frecuencia detectada y cada Pokémon recibido. Si no recibe, comprueba que el nivel cambia al hablar y prueba primero un WAV cerca del micrófono.

El programa anterior permanece en el entorno `m5stack-cores3`, con documentación en [docs/INFRARED_LEGACY.md](docs/INFRARED_LEGACY.md). Para restaurarlo: `run.bat -Env m5stack-cores3 -Usb COM3`.

Procedencia del hardware y de los sprites anteriores: [THIRD_PARTY.md](THIRD_PARTY.md). Esta prueba no utiliza los sprites.
