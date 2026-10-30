#pragma once
#include <stdint.h>
#include <algorithm>

namespace coop {

enum class Element : uint8_t { Grass = 0, Fire = 1, Water = 2, Electric = 3, Neutral = 4 };
enum class ActionType : uint8_t { Attack = 0, Shield = 1, Heal = 2, ComboUlt = 3 };
enum class BattleState : uint8_t { Lobby, Fighting, Victory, Defeat, LinkLost };
enum class Personality : uint8_t { Support = 0, Aggressive = 1, Protector = 2, Strategist = 3 };

inline Personality personalityFromPokemon(uint8_t mon) {
    switch (mon % 4) {
        case 0: return Personality::Support;    // Bulbasaur: heals, team player
        case 1: return Personality::Aggressive; // Charmander: fiery DPS attacker
        case 2: return Personality::Protector;  // Squirtle: shields, defensive tank
        default: return Personality::Strategist;// Pikachu: synergy combos & timing
    }
}

inline const char* personalityName(Personality p) {
    switch (p) {
        case Personality::Support: return "Soporte (Empático)";
        case Personality::Aggressive: return "Atacante (Feroz)";
        case Personality::Protector: return "Protector (Defensor)";
        case Personality::Strategist: return "Estratega (Sinergia)";
    }
    return "Neutral";
}

struct PlayerState {
    uint16_t id = 0;
    uint8_t pokemon = 0;       // 0=Bulbasaur, 1=Charmander, 2=Squirtle, 3=Pikachu
    Element element = Element::Grass;
    Personality personality = Personality::Support;
    uint8_t level = 1;
    int16_t hp = 100;
    int16_t maxHp = 100;
    uint8_t shieldActive = 0;   // 0=no, 1=active (reduces next hit by 50%)
    uint8_t comboCharge = 0;    // 0..100%
    uint32_t nextActionTime = 0;// Autonomous turn timer
    bool isAlive() const { return hp > 0; }
};

struct BossState {
    const char* name = "Mecha-Mewtwo";
    Element element = Element::Neutral;
    int16_t hp = 300;
    int16_t maxHp = 300;
    uint8_t chargeTurn = 0;     // 1 if preparing a super attack
    uint32_t nextAttackTime = 0;
    bool isAlive() const { return hp > 0; }
};

struct ActionEvent {
    uint16_t senderId;
    ActionType type;
    Element element;
    int16_t value;              // damage dealt or hp restored
    bool isCombo;
    const char* shout;          // Robot vocal/text shout
};

struct AutonomousDecision {
    ActionType action;
    const char* shout;
    const char* reason;
};

// Calculates base damage scaling with elemental affinities & level
inline int calculateDamage(Element atkElem, Element defElem, uint8_t atkLevel, uint8_t basePower = 20) {
    float multiplier = 1.0f;
    // Classic triangle: Fire > Grass > Water > Fire; Electric > Water
    if (atkElem == Element::Fire && defElem == Element::Grass) multiplier = 2.0f;
    else if (atkElem == Element::Grass && defElem == Element::Water) multiplier = 2.0f;
    else if (atkElem == Element::Water && defElem == Element::Fire) multiplier = 2.0f;
    else if (atkElem == Element::Electric && defElem == Element::Water) multiplier = 2.0f;
    else if (atkElem == Element::Fire && defElem == Element::Water) multiplier = 0.5f;
    else if (atkElem == Element::Water && defElem == Element::Grass) multiplier = 0.5f;
    else if (atkElem == Element::Grass && defElem == Element::Fire) multiplier = 0.5f;

    float levelScale = 1.0f + (atkLevel - 1) * 0.04f; // +4% per level
    return static_cast<int>(basePower * multiplier * levelScale);
}

inline bool isSynergyCombo(Element first, Element second) {
    return (first == Element::Water && (second == Element::Grass || second == Element::Electric)) ||
           (first == Element::Fire && second == Element::Electric) ||
           (first == Element::Grass && second == Element::Fire);
}

class RaidEngine {
public:
    BattleState state = BattleState::Lobby;
    PlayerState players[2];
    BossState boss;
    uint8_t playerCount = 0;
    Element pendingComboElement = Element::Neutral;
    uint32_t comboWindowExpiry = 0;
    ActionEvent lastEvent{};
    uint32_t totalDamageDealt = 0;
    uint16_t totalTurns = 0;
    bool dirty = true;

    void initRaid(uint8_t difficultyLevel = 1) {
        state = BattleState::Lobby;
        boss.name = difficultyLevel >= 5 ? "Shadow-Rayquaza" : "Raid-Mewtwo";
        boss.maxHp = 220 + difficultyLevel * 80;
        boss.hp = boss.maxHp;
        boss.chargeTurn = 0;
        boss.nextAttackTime = 0;
        pendingComboElement = Element::Neutral;
        comboWindowExpiry = 0;
        totalDamageDealt = 0;
        totalTurns = 0;
        playerCount = 0;
        dirty = true;
    }

    bool addPlayer(uint16_t id, uint8_t pokemonIdx, uint8_t level) {
        if (playerCount >= 2) return false;
        PlayerState& p = players[playerCount++];
        p.id = id;
        p.pokemon = pokemonIdx;
        p.level = std::max<uint8_t>(1, level);
        p.element = static_cast<Element>(pokemonIdx % 4);
        p.personality = personalityFromPokemon(pokemonIdx);
        p.maxHp = 100 + (p.level - 1) * 8;
        p.hp = p.maxHp;
        p.shieldActive = 0;
        p.comboCharge = 0;
        p.nextActionTime = 0;
        dirty = true;
        return true;
    }

    void startBattle(uint32_t now) {
        if (playerCount > 0) {
            state = BattleState::Fighting;
            boss.nextAttackTime = now + 4000;
            for (int i = 0; i < playerCount; ++i) {
                // Stagger autonomous decision times slightly so they don't act in the exact same millisecond
                players[i].nextActionTime = now + 1200 + i * 800;
            }
            dirty = true;
        }
    }

    // AUTONOMOUS AI: The robot decides what to do based on personality and live battlefield context
    AutonomousDecision decideAutonomousAction(uint16_t playerId, uint32_t now) const {
        int pIdx = -1;
        for (int i = 0; i < playerCount; ++i) {
            if (players[i].id == playerId) { pIdx = i; break; }
        }
        if (pIdx < 0 || !players[pIdx].isAlive()) {
            return { ActionType::Attack, "...", "Fainted" };
        }

        const PlayerState& self = players[pIdx];
        const PlayerState& partner = players[1 - pIdx];
        const bool partnerAlive = (playerCount > 1 && partner.isAlive());

        // 1. CRITICAL THREAT: Boss is charging a massive wipeout attack!
        if (boss.chargeTurn == 1 && !self.shieldActive) {
            return { ActionType::Shield, "¡Cuidado! ¡Escudo de equipo!", "Boss charging heavy attack" };
        }

        // 2. CRITICAL ALLY IN DANGER: Partner is critically low on health!
        if (partnerAlive && partner.hp <= partner.maxHp * 0.35f) {
            if (self.personality == Personality::Support || self.personality == Personality::Protector || self.hp > self.maxHp * 0.5f) {
                return { ActionType::Heal, "¡Aguanta compañero! ¡Te curo!", "Partner HP critical" };
            }
        }

        // 3. COMBO SYNERGY OPPORTUNITY: Partner opened an elemental window!
        if (now <= comboWindowExpiry && pendingComboElement != Element::Neutral) {
            if (isSynergyCombo(pendingComboElement, self.element)) {
                return { ActionType::Attack, "¡Aprovecha la sinergia! ¡COMBO!", "Elemental synergy combo" };
            }
        }

        // 4. DUO ULTIMATE ATTACK: 100% team gauge charged!
        if (self.comboCharge >= 100) {
            return { ActionType::ComboUlt, "¡Ataque Definitivo en Pareja!", "100% Ultimate meter charged" };
        }

        // 5. PERSONALITY-DRIVEN TACTICS:
        switch (self.personality) {
            case Personality::Support:
                // If partner or self has taken notable damage, restore health
                if (partnerAlive && partner.hp < partner.maxHp * 0.70f) {
                    return { ActionType::Heal, "¡Recuperando energía!", "Support healing wounded ally" };
                }
                if (self.hp < self.maxHp * 0.50f) {
                    return { ActionType::Heal, "¡Curándome!", "Support self-healing" };
                }
                return { ActionType::Attack, "¡Látigo Cepa!", "Support regular attack" };

            case Personality::Protector:
                // Deploy shield if boss attack is arriving very soon (<1.5s)
                if (boss.nextAttackTime > now && (boss.nextAttackTime - now) <= 1500 && !self.shieldActive) {
                    return { ActionType::Shield, "¡Protegiendo al equipo!", "Protector intercepting boss" };
                }
                return { ActionType::Attack, "¡Pistola Agua!", "Protector standard attack" };

            case Personality::Strategist:
                // Prepares combo or attacks
                return { ActionType::Attack, "¡Rayo Eléctrico!", "Strategist offensive push" };

            case Personality::Aggressive:
            default:
                // Aggressive attacks relentlessly
                return { ActionType::Attack, "¡Lanzallamas directo!", "Aggressive all-out attack" };
        }
    }

    // Execute an action (either decided autonomously by robot or commanded by player)
    bool executePlayerAction(uint16_t playerId, ActionType action, uint32_t now, const char* customShout = nullptr) {
        if (state != BattleState::Fighting) return false;
        int pIdx = -1;
        for (int i = 0; i < playerCount; ++i) {
            if (players[i].id == playerId && players[i].isAlive()) { pIdx = i; break; }
        }
        if (pIdx < 0) return false;

        PlayerState& self = players[pIdx];
        PlayerState& partner = players[1 - pIdx];
        ++totalTurns;

        if (action == ActionType::Attack) {
            bool isCombo = false;
            int dmg = calculateDamage(self.element, boss.element, self.level, 22);

            // Check if completing an active combo window
            if (now <= comboWindowExpiry && pendingComboElement != Element::Neutral) {
                if (isSynergyCombo(pendingComboElement, self.element)) {
                    dmg = static_cast<int>(dmg * 1.6f); // 60% combo bonus
                    isCombo = true;
                    self.comboCharge = std::min<uint8_t>(100, self.comboCharge + 35);
                    if (partner.isAlive()) partner.comboCharge = std::min<uint8_t>(100, partner.comboCharge + 35);
                }
                pendingComboElement = Element::Neutral;
                comboWindowExpiry = 0;
            } else {
                // Open synergy window for partner
                pendingComboElement = self.element;
                comboWindowExpiry = now + 3500; // 3.5s window
                self.comboCharge = std::min<uint8_t>(100, self.comboCharge + 15);
            }

            boss.hp = std::max<int16_t>(0, boss.hp - dmg);
            totalDamageDealt += dmg;
            const char* shout = customShout ? customShout : (isCombo ? "¡Combo Elemental!" : "¡Ataque!");
            lastEvent = { self.id, ActionType::Attack, self.element, static_cast<int16_t>(dmg), isCombo, shout };
            dirty = true;

            if (!boss.isAlive()) {
                state = BattleState::Victory;
                return true;
            }
        } else if (action == ActionType::Shield) {
            self.shieldActive = 1;
            if (partner.isAlive()) partner.shieldActive = 1;
            const char* shout = customShout ? customShout : "¡Escudo de defensa!";
            lastEvent = { self.id, ActionType::Shield, self.element, 50, false, shout };
            dirty = true;
        } else if (action == ActionType::Heal) {
            int healAmount = 25 + self.level * 2;
            if (partner.isAlive() && partner.hp < partner.maxHp) {
                partner.hp = std::min<int16_t>(partner.maxHp, partner.hp + healAmount);
            } else {
                self.hp = std::min<int16_t>(self.maxHp, self.hp + healAmount);
            }
            const char* shout = customShout ? customShout : "¡Cura de apoyo!";
            lastEvent = { self.id, ActionType::Heal, self.element, static_cast<int16_t>(healAmount), false, shout };
            dirty = true;
        } else if (action == ActionType::ComboUlt && self.comboCharge >= 100) {
            self.comboCharge = 0;
            if (partner.isAlive()) partner.comboCharge = 0;
            int ultDmg = calculateDamage(self.element, boss.element, self.level, 60);
            boss.hp = std::max<int16_t>(0, boss.hp - ultDmg);
            totalDamageDealt += ultDmg;
            const char* shout = customShout ? customShout : "¡Ataque Definitivo Dúo!";
            lastEvent = { self.id, ActionType::ComboUlt, self.element, static_cast<int16_t>(ultDmg), true, shout };
            dirty = true;
            if (!boss.isAlive()) {
                state = BattleState::Victory;
                return true;
            }
        }

        // Schedule robot's next autonomous action (e.g. 2.5 to 3.5 seconds later)
        self.nextActionTime = now + 2500 + (rand() % 1000);
        return true;
    }

    // Game loop tick: updates combo timers, autonomous robot actions, and boss turns
    void tick(uint32_t now) {
        if (state != BattleState::Fighting) return;

        // Expire combo window
        if (pendingComboElement != Element::Neutral && now > comboWindowExpiry) {
            pendingComboElement = Element::Neutral;
            dirty = true;
        }

        // Autonomous Robot Action Loop: Each living robot acts when its turn timer expires
        for (int i = 0; i < playerCount; ++i) {
            PlayerState& p = players[i];
            if (p.isAlive() && now >= p.nextActionTime) {
                AutonomousDecision dec = decideAutonomousAction(p.id, now);
                executePlayerAction(p.id, dec.action, now, dec.shout);
                return; // process one action per tick
            }
        }

        // Boss turn
        if (now >= boss.nextAttackTime) {
            executeBossTurn(now);
        }
    }

    uint32_t calculateRewardXp() const {
        if (state != BattleState::Victory) return 30; // consolation XP
        uint32_t base = 150 + (boss.maxHp / 5);
        return base;
    }

private:
    void executeBossTurn(uint32_t now) {
        int targetIdx = -1;
        int16_t minHp = 9999;
        for (int i = 0; i < playerCount; ++i) {
            if (players[i].isAlive() && players[i].hp < minHp) {
                minHp = players[i].hp;
                targetIdx = i;
            }
        }
        if (targetIdx < 0) {
            state = BattleState::Defeat;
            dirty = true;
            return;
        }

        PlayerState& target = players[targetIdx];
        int baseBossDmg = 18 + (totalTurns / 4);

        if (boss.chargeTurn) {
            baseBossDmg *= 2;
            boss.chargeTurn = 0;
        } else if ((totalTurns % 5) == 0) {
            boss.chargeTurn = 1;
        }

        if (target.shieldActive) {
            baseBossDmg /= 2;
            target.shieldActive = 0;
        }

        target.hp = std::max<int16_t>(0, target.hp - baseBossDmg);
        lastEvent = { 0xFFFF /* Boss ID */, ActionType::Attack, boss.element, static_cast<int16_t>(baseBossDmg), boss.chargeTurn == 1,
                      boss.chargeTurn ? "¡Ataque Cargado del Jefe!" : "¡Golpe del Jefe!" };
        dirty = true;

        bool anyAlive = false;
        for (int i = 0; i < playerCount; ++i) {
            if (players[i].isAlive()) anyAlive = true;
        }
        if (!anyAlive) {
            state = BattleState::Defeat;
        } else {
            boss.nextAttackTime = now + 3500 + (rand() % 1000);
        }
    }
};

} // namespace coop
