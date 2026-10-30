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
    assert(engine.players[0].personality == Personality::Aggressive);
    assert(engine.players[0].maxHp == 100 + (3 - 1) * 8);

    bool p2 = engine.addPlayer(0x1002, 2 /* Squirtle = Water */, 2);
    assert(p2);
    assert(engine.playerCount == 2);
    assert(engine.players[1].personality == Personality::Protector);

    puts("PASS: testRaidInit");
}

void testAutonomousDecisions() {
    RaidEngine engine;
    engine.initRaid(1);
    engine.addPlayer(0x1001, 0 /* Bulbasaur = Support */, 1);
    engine.addPlayer(0x1002, 1 /* Charmander = Aggressive */, 1);
    engine.startBattle(1000);

    // Scenario 1: Boss is charging a heavy wipeout attack!
    engine.boss.chargeTurn = 1;
    AutonomousDecision d1 = engine.decideAutonomousAction(0x1001, 1100);
    assert(d1.action == ActionType::Shield);
    printf("  [AI Decision Boss Charge]: %s -> %s\n", d1.shout, d1.reason);

    // Reset charge
    engine.boss.chargeTurn = 0;

    // Scenario 2: Partner is critically low on health! (Partner HP = 20 / 100)
    engine.players[1].hp = 20;
    AutonomousDecision d2 = engine.decideAutonomousAction(0x1001, 1200); // Bulbasaur deciding
    assert(d2.action == ActionType::Heal);
    printf("  [AI Decision Ally Low]: %s -> %s\n", d2.shout, d2.reason);

    // Scenario 3: Partner opened a synergy window with Fire!
    engine.players[1].hp = 100;
    engine.pendingComboElement = Element::Grass;
    engine.comboWindowExpiry = 3000;
    AutonomousDecision d3 = engine.decideAutonomousAction(0x1002, 1500); // Charmander (Fire) can combo with Grass!
    assert(d3.action == ActionType::Attack);
    printf("  [AI Decision Combo Window]: %s -> %s\n", d3.shout, d3.reason);

    // Scenario 4: Duo Ultimate ready (100%)
    engine.players[0].comboCharge = 100;
    AutonomousDecision d4 = engine.decideAutonomousAction(0x1001, 1600);
    assert(d4.action == ActionType::ComboUlt);
    printf("  [AI Decision Ultimate 100%%]: %s -> %s\n", d4.shout, d4.reason);

    puts("PASS: testAutonomousDecisions");
}

void testComboSynergy() {
    RaidEngine engine;
    engine.initRaid(1);
    engine.addPlayer(0x1001, 2 /* Water = Squirtle */, 1);
    engine.addPlayer(0x1002, 0 /* Grass = Bulbasaur */, 1);
    engine.startBattle(1000);

    int initialBossHp = engine.boss.hp;

    // Player 1 attacks with Water at t=1000
    bool ok1 = engine.executePlayerAction(0x1001, ActionType::Attack, 1000);
    assert(ok1);
    assert(engine.lastEvent.isCombo == false);
    assert(engine.pendingComboElement == Element::Water);

    // Player 2 attacks with Grass at t=2000 (within 3.5s synergy window!)
    bool ok2 = engine.executePlayerAction(0x1002, ActionType::Attack, 2000);
    assert(ok2);
    assert(engine.lastEvent.isCombo == true);
    puts("PASS: testComboSynergy");
}

void testAutonomousTickLoop() {
    RaidEngine engine;
    engine.initRaid(1);
    engine.addPlayer(0x1001, 1 /* Charmander */, 1);
    engine.addPlayer(0x1002, 0 /* Bulbasaur */, 1);
    engine.startBattle(1000);

    int bossHpInitial = engine.boss.hp;

    // Simulate 10 seconds of automated battle
    for (uint32_t t = 1000; t <= 12000; t += 200) {
        engine.tick(t);
        if (engine.state == BattleState::Victory || engine.state == BattleState::Defeat) break;
    }

    assert(engine.totalTurns > 0);
    assert(engine.boss.hp < bossHpInitial);
    printf("  [Autonomous Battle Result]: Boss HP %d/%d, Turns taken: %u\n",
           engine.boss.hp, engine.boss.maxHp, engine.totalTurns);
    puts("PASS: testAutonomousTickLoop");
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
    testAutonomousDecisions();
    testComboSynergy();
    testAutonomousTickLoop();
    testMotionController();
    puts("ALL AUTONOMOUS & CO-OP BATTLE TESTS PASSED!");
    return 0;
}
