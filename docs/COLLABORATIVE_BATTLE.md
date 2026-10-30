# Collaborative Pokémon Battle Engine · Stack-chan

Este documento detalla la arquitectura y diseño del nuevo sistema de **combate colaborativo** implementado en la rama `feature/collaborative-battle`, diseñado para que **los Pokémon sean los propios robots Stack-chan** en lugar de meras consolas con animaciones pasivas.

---

## 1. Concepto y Visión

Dos o más robots Stack-chan se unen como compañeros de equipo para enfrentarse a un **Jefe de Incursión (Raid Boss)** o en combates por equipos cooperativos:
- **El robot ES el Pokémon**: El Stack-chan reacciona físicamente con sus servomotores al atacar, recibir golpes, recargar combos o celebrar la victoria.
- **Roles y Trabajo en Equipo**: Un jugador puede actuar como **Atacante** mientras su compañero actúa como **Soporte/Defensor** curando y activando escudos.
- **Sinergias Elementales**: Los ataques consecutivos abren "ventanas de combo" que multiplican el daño y recargan la habilidad definitiva del equipo.

```
                  ┌───────────────────────────────┐
                  │       JEFE DE INCURSIÓN       │
                  │   "Mecha-Mewtwo" / Rayquaza   │
                  │         [ HP: 300 ]           │
                  └───────▲───────────────▲───────┘
                          │               │
                 Ataque   │               │   Ataque Sinergia
                 Agua     │               │   Planta (+60% COMBO)
                          │               │
               ┌──────────┴──────┐ ┌──────┴──────────┐
               │   Stack-chan 1  │ │   Stack-chan 2  │
               │  "Squirtle-chan"│ │  "Bulba-chan"   │
               │    (Atacante)   │ │    (Soporte)    │
               └────────┬────────┘ └────────┬────────┘
                        │                   │
                        └── Escudo / Cura ──┘
```

---

## 2. Mecánicas de Combate

### A. Sinergia Elemental y Combos
1. Cuando el Jugador 1 lanza un ataque elemental (ej. Agua), se abre una **ventana de sinergia de 3,5 segundos**.
2. Si el Jugador 2 ataca durante esa ventana con un elemento compatible (Planta o Eléctrico), se desata un **Combo Elemental**:
   - Daño incrementado en un **+60%**.
   - Carga de la Barra Definitiva (Ultimate) para ambos jugadores (+35%).
3. **Ataque Definitivo (Combo Ult)**: Al alcanzar el 100% de carga entre ambos, pueden desatar una descarga combinada de 60 de daño base.

### B. Habilidades de Equipo
En cada turno el jugador puede elegir mediante la pantalla táctil o tarjetas NFC:
- ⚔️ **Ataque**: Inflige daño elemental al jefe y abre o completa una ventana de combo.
- 🛡️ **Defensa / Escudo**: Activa una barrera que absorbe el 50% del siguiente ataque del jefe para todo el equipo.
- 💊 **Cura / Apoyo**: Restaura puntos de vida (+25 PS + bonus de nivel) al compañero más herido.

---

## 3. Coreografía Física de los Servos (`include/battle_motion.h`)

Para dar vida al robot como criatura de combate, el motor incorpora poses y movimientos fluidos:

| Estado de Combate | Acción de Servos (Pan / Tilt) | Expresión & Luces LED |
| :--- | :--- | :--- |
| **Ataque Normal** | Embestida rápida hacia adelante (cabeza se inclina rápidamente al frente y recupera centro). | Ojos encendidos, LED base pulsa el color elemental. |
| **Daño Recibido** | La cabeza retrocede y tiembla de lado a lado. | Ojos parpadean, LED base en rojo. |
| **Carga de Combo** | La cabeza se echa hacia atrás mirando hacia arriba mientras acumula energía. | LEDs de la base aumentan de brillo progresivamente. |
| **Victoria** | Asentimiento alegre y bamboleo rítmico de izquierda a derecha. | LEDs arcoíris, ojos sonrientes. |
| **Debilitado (0 PS)**| La cabeza cae inerte hacia abajo (tilt máximo al suelo). | Luces apagadas, expresión dormida/cruz. |

---

## 4. Progresión y Niveles Centralizados

El motor calcula la escala de estadísticas por nivel para integrarse con el servidor central:
- **Puntos de Salud**: $\text{PS Máximos} = 100 + (\text{Nivel} - 1) \times 8$.
- **Potencia de Ataque**: $+4\%$ de daño adicional por nivel de entrenador.
- **Recompensas de Experiencia (XP)**:
  - Victoria en Incursión: $+150\text{ XP} + (\text{PS Jefe} / 5)$.
  - Derrota consolación: $+30\text{ XP}$.

---

## 5. Pruebas Automatizadas

El motor cuenta con su propia suite de pruebas unitarias nativas en [tests/coop_battle_test.cpp](file:///Users/lluismasruiz/Desktop/StackChan-Course/Pokemon-battle-m5stack/tests/coop_battle_test.cpp). Para ejecutarlas:

```bash
clang++ -std=c++17 -I include tests/coop_battle_test.cpp -o tests/coop_battle_test
./tests/coop_battle_test
```
