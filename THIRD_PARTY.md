# Procedencia

## Hardware

`lib/StackChanHW/` procede de [rhyloo/StackChan-Course](https://github.com/rhyloo/StackChan-Course), commit `8746558b70cafb7d2812e96a3283c062c97b0a73`. El programa parte del lector NFC y del trabajo de Pokedex sobre ese curso. El proyecto original no contenía un archivo LICENSE en la raíz al preparar esta copia; esta referencia no concede derechos adicionales sobre ese código.

PlatformIO descarga M5Unified, M5GFX, M5Unit-NFC, M5UnitUnified, IRremoteESP8266 y Arduino-ESP32 con sus correspondientes avisos y licencias.

## Animaciones

Los GIF de Bulbasaur, Charmander y Squirtle proceden de [PokeAPI/sprites](https://github.com/PokeAPI/sprites), carpeta `sprites/pokemon/versions/generation-v/black-white/animated/`, archivos `1.gif`, `4.gif` y `7.gif`.

`src/animations.h` contiene sus fotogramas convertidos a JPEG con `tools/prepare_assets.py`. Pokémon y los personajes pertenecen a sus respectivos titulares. Este proyecto no es oficial ni está afiliado a ellos.

No se incluye una licencia nueva que pretenda relicenciar estos recursos de terceros.
