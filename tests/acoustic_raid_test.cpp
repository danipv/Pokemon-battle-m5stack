#include <assert.h>
#include <stdio.h>
#include <vector>
#include "tone_combat_protocol.h"
#include "coop_battle.h"

using namespace acoustic;
using namespace coop;

// Synthesizes a 3-tone acoustic sequence [START -> PAYLOAD -> END] into 16 kHz audio blocks
void synthesizeActionAudio(CombatAction act, std::vector<std::vector<int16_t>>& outputBlocks) {
    const int payloadTone = actionToTone(act);
    const int sequenceTones[3] = { TONE_START, payloadTone, TONE_END };

    for (int t = 0; t < 3; ++t) {
        int hz = FREQUENCIES[sequenceTones[t]];
        // Generate NOTE_MS (250 ms = ~12 blocks of 20 ms)
        for (int b = 0; b < 12; ++b) {
            std::vector<int16_t> block(BLOCK);
            for (size_t i = 0; i < BLOCK; ++i) {
                float sampleTime = static_cast<float>(b * BLOCK + i) / RATE;
                block[i] = static_cast<int16_t>(10000.0f * sinf(6.28318530718f * hz * sampleTime));
            }
            outputBlocks.push_back(block);
        }
        // Silence GAP_MS (80 ms = 4 blocks)
        for (int b = 0; b < 4; ++b) {
            std::vector<int16_t> silence(BLOCK, 0);
            outputBlocks.push_back(silence);
        }
    }
}

void testAcousticTransmissionAndDecoding() {
    CombatToneDecoder decoder;
    decoder.reset();

    for (int a = 0; a <= 5; ++a) {
        CombatAction expectedAction = static_cast<CombatAction>(a);
        std::vector<std::vector<int16_t>> audioBlocks;
        synthesizeActionAudio(expectedAction, audioBlocks);

        int decodedResult = -1;
        uint32_t now = 1000;
        for (const auto& block : audioBlocks) {
            int res = decoder.consume(block.data(), now);
            if (res >= 0) {
                decodedResult = res;
            }
            now += 20; // 20 ms per block
        }

        assert(decodedResult == a);
        printf("  [Acoustic Decode]: Decoded %d -> %s\n", a, actionDescription(expectedAction));
    }
    puts("PASS: testAcousticTransmissionAndDecoding");
}

void testEndToEndAutonomousAcousticBattle() {
    RaidEngine engine;
    engine.initRaid(1);
    engine.addPlayer(0x1001, 2 /* Squirtle = Water */, 1);
    engine.addPlayer(0x1002, 0 /* Bulbasaur = Grass */, 1);
    engine.startBattle(1000);

    CombatToneDecoder robot1Ear; // Robot 1 microphone listener
    CombatToneDecoder robot2Ear; // Robot 2 microphone listener
    robot1Ear.reset();
    robot2Ear.reset();

    int initialBossHp = engine.boss.hp;

    // STEP 1: Robot 1 (Squirtle) autonomously decides to use Water Attack!
    AutonomousDecision d1 = engine.decideAutonomousAction(0x1001, 1000);
    assert(d1.action == ActionType::Attack);
    CombatAction act1 = CombatAction::AttackWater;

    // Robot 1 speaks acoustic tones into the air
    std::vector<std::vector<int16_t>> r1Audio;
    synthesizeActionAudio(act1, r1Audio);

    // Robot 2 hears it through its microphone!
    int heardByRobot2 = -1;
    uint32_t now = 1000;
    for (const auto& blk : r1Audio) {
        int r = robot2Ear.consume(blk.data(), now);
        if (r >= 0) heardByRobot2 = r;
        now += 20;
    }
    assert(heardByRobot2 == static_cast<int>(CombatAction::AttackWater));
    printf("  [Audio RX Robot 2]: Heard partner Water Attack!\n");

    // Engine registers Robot 1's action
    engine.executePlayerAction(0x1001, ActionType::Attack, now, d1.shout);
    assert(engine.pendingComboElement == Element::Water);

    // STEP 2: Robot 2 hears partner and immediately decides to COMBO with Grass Attack!
    AutonomousDecision d2 = engine.decideAutonomousAction(0x1002, now);
    assert(d2.action == ActionType::Attack); // Combo triggered!

    // Robot 2 speaks acoustic tones back
    CombatAction act2 = CombatAction::AttackGrass;
    std::vector<std::vector<int16_t>> r2Audio;
    synthesizeActionAudio(act2, r2Audio);

    // Robot 1 hears Robot 2 through its microphone!
    int heardByRobot1 = -1;
    for (const auto& blk : r2Audio) {
        int r = robot1Ear.consume(blk.data(), now);
        if (r >= 0) heardByRobot1 = r;
        now += 20;
    }
    assert(heardByRobot1 == static_cast<int>(CombatAction::AttackGrass));
    printf("  [Audio RX Robot 1]: Heard partner Grass Combo Response!\n");

    // Engine registers Robot 2's action -> COMBO ACHIEVED!
    engine.executePlayerAction(0x1002, ActionType::Attack, now, d2.shout);
    assert(engine.lastEvent.isCombo == true);
    assert(engine.boss.hp < initialBossHp);

    printf("  [End-to-End Success]: Autonomous acoustic coordination complete! Boss HP: %d/%d\n",
           engine.boss.hp, engine.boss.maxHp);
    puts("PASS: testEndToEndAutonomousAcousticBattle");
}

int main() {
    testAcousticTransmissionAndDecoding();
    testEndToEndAutonomousAcousticBattle();
    puts("ALL ACOUSTIC RAID TESTS PASSED!");
    return 0;
}
