# Pokémon Battle · M5Stack StackChan

Dos StackChan, dos tarjetas NFC y un combate por **infrarrojos**, sin Wi-Fi, internet ni microSD. Cada jugador elige **Bulbasaur, Charmander o Squirtle** acercando su tarjeta a la base. Los robots se encuentran y el combate empieza automáticamente.

## Necesitas

- Dos StackChan K151 con CoreS3 y sus bases con NFC e infrarrojos.
- El mismo firmware de este repositorio en ambos robots.
- Tres tarjetas NFC-A distintas por jugador (NTAG213/215/216, MIFARE…). También podéis usar un mismo juego de tarjetas por turnos.
- USB-C de datos para instalar. Después el combate funciona sin ordenador.

## Instalación en Windows

Instala [Python 3.10 o posterior](https://www.python.org/downloads/windows/) (marca añadir Python al PATH) y [Git](https://git-scm.com/download/win), si todavía no los tienes. También puedes descargar el ZIP del repositorio.

```powershell
git clone https://github.com/danipv/Pokemon-battle-m5stack.git
cd Pokemon-battle-m5stack
.\setup.bat
.\run.bat -Usb COM3
```

Sustituye `COM3` por el puerto de tu robot, visible en el Administrador de dispositivos. Sin `-Usb` PlatformIO intenta detectarlo. La primera compilación descarga el framework y compiladores y puede tardar varios minutos. `setup.bat` crea `.venv`, instala dependencias y compila; `run.bat` compila y carga por USB.

Para ver el registro USB tras instalar: `run.bat -Usb COM3 -NoFlash -Monitor`. Cierra cualquier otro monitor que esté usando ese puerto. Si no se puede cargar, mantén RST unos tres segundos hasta el LED verde y vuelve a intentarlo.

Con PlatformIO ya instalado (Windows, Linux o macOS):

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

En Windows se usa una carpeta corta para el compilador (`%USERPROFILE%\.pokemon-pio`) para evitar errores de longitud de ruta. Puedes elegir otra con `setup.bat -CoreDir RUTA`. La ruta local queda en `.pio-core`, excluida de Git.

## Tarjetas: conserva la Pokedex anterior

El firmware lee **el mismo espacio NVS `pokemon-nfc` y clave `cards`** de la Pokedex de tres iniciales. Si ya las configuraste en ese robot, las reconoce al actualizar por USB. **No ejecutes `erase_flash`** ni cambies la tabla de particiones si quieres conservarlas.

En un robot nuevo aparece el asistente:

1. Acerca una tarjeta para Bulbasaur y retírala.
2. Acerca otra para Charmander y retírala.
3. Acerca una tercera para Squirtle.

Rechaza tarjetas repetidas. Guarda las tres asignaciones juntas y las conserva al apagar. **No modifica el contenido de las tarjetas** ni necesita NDEF. Cada robot guarda sus propias asignaciones, sin compartir UID por infrarrojos.

Pulsa **Tarjetas** para cambiarlas; **Cancelar / volver** mantiene las anteriores hasta completar las tres nuevas. Retira una tarjeta aproximadamente dos segundos antes de volver a presentarla.

## Jugar

1. Coloca los robots cerca, con **el emisor y receptor IR de las bases orientados entre sí**, sin obstáculos. Empieza a unos 10–30 cm y ajusta la posición; no basta con que las caras se miren. Evita sol directo.
2. Cada jugador acerca su tarjeta. La pantalla indica **Buscando rival por infrarrojos**.
3. Cuando se reconocen aparecen ambos Pokémon animados y sus barras de vida.
4. El combate es **automático, por turnos**. Empieza con 100 PS por Pokémon. El primer turno se decide usando los identificadores aleatorios de la partida.
5. Fuego gana ventaja contra Planta; Planta contra Agua; Agua contra Fuego. El daño base varía de 12 a 18: con ventaja se duplica y con desventaja se reduce a la mitad. Gana quien deje al rival a 0 PS.
6. Ambos robots muestran victoria/derrota. Pulsa **Salir / nuevo** y vuelve a acercar una tarjeta para otra partida.

Puedes cambiar de Pokémon durante la búsqueda pasando otra tarjeta; durante un combate la elección queda bloqueada. Pulsa **Salir / nuevo** para cancelarlo. Si se pierde el enlace, se detiene y muestra **Sin enlace**, sin inventar un ganador.

Solo se emparejan **dos robots por zona de juego**: para varias parejas, sepáralas físicamente. No es un sistema de torneos ni tiene selección de rival entre múltiples dispositivos.

### Si no se encuentran

Durante la búsqueda aparecen los contadores **IR TX / RX / OK**:

- **TX**: paquetes que el programa ha enviado al GPIO. No prueba por sí solo que el LED emita luz.
- **RX**: señales recibidas, incluidas las de mandos y tramas que no se reconocen.
- **OK**: paquetes válidos de este juego. Un mando de TV puede aumentar RX sin aumentar OK.

Si TX aumenta y RX permanece a cero, pulsa **Salir / nuevo** para dejar el robot solo escuchando y prueba un mando infrarrojo contra la base. Si el mando aumenta RX, el receptor responde: comprueba la emisión y orientación del otro robot. Una cámara de móvil que pueda ver los destellos del mando puede servir para comparar con el LED del robot; algunas cámaras filtran infrarrojos.

El registro USB detalla protocolo, bits y trama recibida. La emisión usa el periférico **RMT del ESP32**, con portadora de 38 kHz y ciclo de trabajo del 50 %; tanto la portadora como las duraciones NEC se generan por hardware. Durante cada envío normal se pausa la captura del receptor y se reanuda después, conservando el temporizador del ESP32 en lugar de destruirlo y volverlo a crear.

Para comprobar físicamente el emisor y el receptor de un solo robot, envía `irtest` seguido de Enter por el monitor serie. Durante 12 segundos emite un código NEC de diagnóstico ajeno al juego y deja el receptor activo. Refleja la luz con una hoja blanca frente a la base. Si recibe ese mismo código, registra `SELFTEST physical echo` y muestra **Eco IR correcto**. No inyecta paquetes simulados: cuenta únicamente ecos capturados por el receptor real. Que no aparezca eco no distingue por sí solo una avería de una reflexión insuficiente.

Según el [esquema de la placa de alimentación](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1205/SCH_Power.pdf), el emisor usa **BUS_5V**; no comparte la salida VM de los servos.

## Cómo se sincroniza

Protocolo v1 sobre **NEC extendido de 32 bits a 38 kHz**, TX GPIO5 y RX GPIO10. Cada paquete lleva una firma y un tipo: HELLO, ACK, SYNC o BYE. HELLO usa un identificador aleatorio de 16 bits y el Pokémon elegido. No se transmite el UID NFC.

Ambos calculan la misma sesión, orden de turnos y daños. Cada turno exige que el otro robot haya confirmado el anterior. Los SYNC se repiten con intervalos aleatorios para reducir colisiones; los duplicados no producen ataques extra. Se apaga la recepción al emitir para evitar recibir el propio LED. Los paquetes de otras sesiones se descartan.

El intervalo mínimo de turno es 1,8 s; se alarga si hace falta resincronizar. Tras 12 s sin recibir señal o 20 s sin avanzar, se cancela el combate. La firma de sesión es corta: este protocolo está pensado para dos juguetes próximos, no para autenticación o juego competitivo seguro.

## Animaciones y memoria

Conserva las tres animaciones originales de la Pokedex. Todos los fotogramas están en flash; se reutiliza un único lienzo de 160 × 128 en PSRAM para dibujar los dos Pokémon. No se guardan fotogramas en NVS, ni se acumula un historial de partidas. No se inicia cámara ni Wi-Fi. Al arrancar, centra la cabeza y la coloca horizontal con las posiciones calibradas de los servos.

Los GIF fuente y sus atribuciones están en `assets/` y [THIRD_PARTY.md](THIRD_PARTY.md). Para regenerarlos: `python -m pip install pillow` y `python tools/prepare_assets.py`.

## Pruebas del protocolo

`include/battle.h` es el motor real utilizado por el firmware y por `tests/battle_test.cpp`. Las pruebas comprueban codificación, tipos, 900 combates con pérdidas/duplicados/retrasos, coincidencia de resultados, paquetes ajenos, cancelación y tiempos de espera.

Con un compilador C++17:

```bash
g++ -std=c++17 -I include tests/battle_test.cpp -o battle_test
./battle_test
```

También puede usarse `zig c++ -std=c++17 -I include tests/battle_test.cpp -o battle_test`. El registro serie muestra el arranque, lecturas NFC, estado de combate, turno y vida.

**Límite de validación:** las pruebas de software no sustituyen una prueba física entre dos bases: la distancia y orientación IR deben comprobarse con ambos robots.
