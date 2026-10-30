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

## 2. Mecánicas de Combate y Toma de Decisiones Autónoma

### A. Los Robots Eligen de Forma Autónoma sus Acciones
Cada Stack-chan posee una **inteligencia táctica contextual** y una **personalidad Pokémon** propia:

| Pokémon | Arquetipo de Personalidad | Comportamiento Táctico Autónomo |
| :--- | :--- | :--- |
| **Bulbasaur** | **Soporte (Empático)** | Prioriza curar al compañero cuando sus PS caen por debajo del 70%, busca sinergias de combo y desgasta con ataques constantes. |
| **Charmander**| **Atacante (Feroz)** | Estilo ofensivo agresivo; prioriza daño masivo, acumula carga definitiva y remata combos elementales. |
| **Squirtle**  | **Protector (Defensor)** | Analiza los tiempos de ataque del jefe y activa escudos de equipo justo antes de los golpes enemigos para proteger al grupo. |
| **Pikachu**   | **Estratega (Sinergia)**| Sincroniza sus ataques en las ventanas de sinergia para desatar combos elementales letales. |

### B. Árbol de Prioridades Tácticas en Tiempo Real
En cada ciclo de combate, el cerebro del robot (`decideAutonomousAction`) evalúa la situación:
1. **Peligro Crítico**: Si el Jefe está preparando un ataque cargado devastador (`boss.chargeTurn == 1`), el robot prioriza **Escudo de Equipo** gritando: *"¡Cuidado! ¡Escudo de equipo!"*.
2. **Compañero en Riesgo**: Si el compañero tiene menos del 35% de PS, el robot utiliza **Cura** exclamando: *"¡Aguanta compañero! ¡Te curo!"*.
3. **Oportunidad de Combo**: Si el compañero acaba de abrir una ventana elemental compatible, aprovecha inmediatamente para disparar un **Combo Elemental (+60% daño)**.
4. **Habilidad Definitiva (Dúo Ult)**: Al alcanzar el 100% de la barra combinada, desatan juntos el ataque definitivo.
5. **Decisión por Personalidad**: Si el combate está estable, cada robot actúa según su naturaleza (atacar, defender o apoyar).

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
