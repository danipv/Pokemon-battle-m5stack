#pragma once
#include <stdint.h>
#include <algorithm>

namespace coop {

enum class Element : uint8_t { Grass = 0, Fire = 1, Water = 2, Electric = 3, Neutral = 4 };
enum class ActionType : uint8_t { Attack = 0, Shield = 1, Heal = 2, ComboUlt = 3 };
enum class BattleState : uint8_t { Lobby, Fighting, Victory, Defeat, LinkLost };

struct PlayerState {
    uint16_t id = 0;
    uint8_t pokemon = 0;       // 0=Bulbasaur, 1=Charmander, 2=Squirtle, 3=Pikachu
    Element element = Element::Grass;
    uint8_t level = 1;
    int16_t hp = 100;
    int16_t maxHp = 100;
    uint8_t shieldActive = 0;   // 0=no, 1=active (reduces next hit by 50%)
    uint8_t comboCharge = 0;    // 0..100%
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
        p.maxHp = 100 + (p.level - 1) * 8;
        p.hp = p.maxHp;
        p.shieldActive = 0;
        p.comboCharge = 0;
        dirty = true;
        return true;
    }

    void startBattle(uint32_t now) {
        if (playerCount > 0) {
            state = BattleState::Fighting;
            boss.nextAttackTime = now + 4000;
            dirty = true;
        }
    }

    // Player action: Attack, Shield, Heal, ComboUlt
    bool executePlayerAction(uint16_t playerId, ActionType action, uint32_t now) {
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
                // Combo synergy triggers if partner used compatible element
                if ((pendingComboElement == Element::Water && (self.element == Element::Grass || self.element == Element::Electric)) ||
                    (pendingComboElement == Element::Fire && self.element == Element::Electric) ||
                    (pendingComboElement == Element::Grass && self.element == Element::Fire)) {
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
            lastEvent = { self.id, ActionType::Attack, self.element, static_cast<int16_t>(dmg), isCombo };
            dirty = true;

            if (!boss.isAlive()) {
                state = BattleState::Victory;
                return true;
            }
        } else if (action == ActionType::Shield) {
            self.shieldActive = 1;
            // Also shields partner if alive
            if (partner.isAlive()) partner.shieldActive = 1;
            lastEvent = { self.id, ActionType::Shield, self.element, 50, false };
            dirty = true;
        } else if (action == ActionType::Heal) {
            int healAmount = 25 + self.level * 2;
            if (partner.isAlive() && partner.hp < partner.maxHp) {
                partner.hp = std::min<int16_t>(partner.maxHp, partner.hp + healAmount);
            } else {
                self.hp = std::min<int16_t>(self.maxHp, self.hp + healAmount);
            }
            lastEvent = { self.id, ActionType::Heal, self.element, static_cast<int16_t>(healAmount), false };
            dirty = true;
        } else if (action == ActionType::ComboUlt && self.comboCharge >= 100) {
            self.comboCharge = 0;
            if (partner.isAlive()) partner.comboCharge = 0;
            int ultDmg = calculateDamage(self.element, boss.element, self.level, 60);
            boss.hp = std::max<int16_t>(0, boss.hp - ultDmg);
            totalDamageDealt += ultDmg;
            lastEvent = { self.id, ActionType::ComboUlt, self.element, static_cast<int16_t>(ultDmg), true };
            dirty = true;
            if (!boss.isAlive()) {
                state = BattleState::Victory;
                return true;
            }
        }
        return true;
    }

    // Boss AI tick: attacks players on schedule
    void tick(uint32_t now) {
        if (state != BattleState::Fighting) return;

        // Expire combo window
        if (pendingComboElement != Element::Neutral && now > comboWindowExpiry) {
            pendingComboElement = Element::Neutral;
            dirty = true;
        }

        // Boss turn
        if (now >= boss.nextAttackTime) {
            executeBossTurn(now);
        }
    }

    // Experience calculation after victory
    uint32_t calculateRewardXp() const {
        if (state != BattleState::Victory) return 30; // consolation XP
        uint32_t base = 150 + (boss.maxHp / 5);
        return base;
    }

private:
    void executeBossTurn(uint32_t now) {
        // Choose target: player with lowest HP or random
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
            // Super charged attack
            baseBossDmg *= 2;
            boss.chargeTurn = 0;
        } else if ((totalTurns % 5) == 0) {
            // Warn charge for next turn
            boss.chargeTurn = 1;
        }

        if (target.shieldActive) {
            baseBossDmg /= 2;
            target.shieldActive = 0;
        }

        target.hp = std::max<int16_t>(0, target.hp - baseBossDmg);
        lastEvent = { 0xFFFF /* Boss ID */, ActionType::Attack, boss.element, static_cast<int16_t>(baseBossDmg), boss.chargeTurn == 1 };
        dirty = true;

        // Check if all players fainted
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
