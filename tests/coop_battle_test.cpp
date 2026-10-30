#include <assert.h>
#include <stdio.h>
#include "coop_battle.h"
#include "battle_motion.h"

using namespace coop;

void testRaidInit() {
    RaidEngine engine;
    engine.initRaid(1);
    assert(engine.state == BattleState::Lobby);
    assert(engine.boss.hp == 300);
    assert(engine.playerCount == 0);

    bool p1 = engine.addPlayer(0x1001, 1 /* Charmander = Fire */, 3);
    assert(p1);
    assert(engine.playerCount == 1);
    assert(engine.players[0].element == Element::Fire);
    assert(engine.players[0].maxHp == 100 + (3 - 1) * 8);

    bool p2 = engine.addPlayer(0x1002, 2 /* Squirtle = Water */, 2);
    assert(p2);
    assert(engine.playerCount == 2);

    // Cannot add a third player in 2-player co-op
    bool p3 = engine.addPlayer(0x1003, 0, 1);
    assert(!p3);
    puts("PASS: testRaidInit");
}

void testComboSynergy() {
    RaidEngine engine;
    engine.initRaid(1);
    engine.addPlayer(0x1001, 2 /* Water = Squirtle */, 1);
    engine.addPlayer(0x1002, 0 /* Grass = Bulbasaur */, 1);
    engine.startBattle(1000);
    assert(engine.state == BattleState::Fighting);

    int initialBossHp = engine.boss.hp;

    // Player 1 attacks with Water at t=1000
    bool ok1 = engine.executePlayerAction(0x1001, ActionType::Attack, 1000);
    assert(ok1);
    assert(engine.lastEvent.isCombo == false);
    assert(engine.pendingComboElement == Element::Water);
    assert(engine.comboWindowExpiry == 1000 + 3500);
    int hpAfterP1 = engine.boss.hp;
    assert(hpAfterP1 < initialBossHp);

    // Player 2 attacks with Grass at t=2000 (within 3.5s synergy window!)
    bool ok2 = engine.executePlayerAction(0x1002, ActionType::Attack, 2000);
    assert(ok2);
    assert(engine.lastEvent.isCombo == true); // Synergy combo triggered!
    int hpAfterP2 = engine.boss.hp;
    int p1Dmg = initialBossHp - hpAfterP1;
    int p2Dmg = hpAfterP1 - hpAfterP2;
    // Combo damage should be boosted (1.6x)
    assert(p2Dmg > p1Dmg);
    puts("PASS: testComboSynergy");
}

void testSupportShieldAndHeal() {
    RaidEngine engine;
    engine.initRaid(1);
    engine.addPlayer(0x1001, 1, 1); // Attacker
    engine.addPlayer(0x1002, 0, 1); // Support
    engine.startBattle(1000);

    // Damage player 1
    engine.players[0].hp = 40;

    // Player 2 casts Heal
    engine.executePlayerAction(0x1002, ActionType::Heal, 1500);
    assert(engine.players[0].hp > 40);

    // Player 2 casts Shield
    engine.executePlayerAction(0x1002, ActionType::Shield, 2000);
    assert(engine.players[0].shieldActive == 1);
    assert(engine.players[1].shieldActive == 1);

    // Boss attacks at t=4000 (shield should absorb half damage and expire)
    int hpBefore = engine.players[0].hp;
    engine.tick(5000); // trigger boss attack
    int hpAfter = engine.players[0].hp;
    int dmgTaken = hpBefore - hpAfter;
    assert(dmgTaken > 0);
    assert(engine.players[0].shieldActive == 0); // consumed
    puts("PASS: testSupportShieldAndHeal");
}

void testVictoryCondition() {
    RaidEngine engine;
    engine.initRaid(1);
    engine.addPlayer(0x1001, 1, 5); // Lv. 5
    engine.startBattle(1000);

    engine.boss.hp = 10;
    engine.executePlayerAction(0x1001, ActionType::Attack, 1200);
    assert(engine.boss.hp == 0);
    assert(engine.state == BattleState::Victory);
    assert(engine.calculateRewardXp() > 100);
    puts("PASS: testVictoryCondition");
}

void testMotionController() {
    motion::MotionController mc;
    assert(mc.currentPose == motion::Pose::Neutral);
    mc.attack(1000);
    assert(mc.currentPose == motion::Pose::LungeAttack);
    mc.update(1200);
    assert(mc.currentPose == motion::Pose::LungeAttack);
    mc.update(1500);
    assert(mc.currentPose == motion::Pose::Neutral);
    puts("PASS: testMotionController");
}

int main() {
    testRaidInit();
    testComboSynergy();
    testSupportShieldAndHeal();
    testVictoryCondition();
    testMotionController();
    puts("ALL CO-OP BATTLE TESTS PASSED SUCCESSFULLY!");
    return 0;
}
