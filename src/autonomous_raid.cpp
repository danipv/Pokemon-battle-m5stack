// Autonomous End-to-End Pokémon Raid Battle for M5Stack Stack-chan
// Robots autonomously decide actions, communicate via acoustic tones,
// execute physical servo choreography, and team up against the Raid Boss.

#include <M5Unified.h>
#include "board.h"
#include "animations.h"
#include "coop_battle.h"
#include "battle_motion.h"
#include "tone_combat_protocol.h"

static constexpr uint16_t BG = 0x08A4;     // RGB(13,22,35)
static constexpr uint16_t MUTED = 0x9D16;
static const char* NAMES[] = { "Bulbasaur", "Charmander", "Squirtle", "Pikachu" };
static const uint16_t COLORS[] = { 0x6E6C, 0xFC29, 0x4DFF, 0xFFE0 };
static const uint8_t LEDS[4][3] = {
    { 0, LED_MAX_LEVEL, 0 },
    { LED_MAX_LEVEL, 20, 0 },
    { 0, 20, LED_MAX_LEVEL },
    { LED_MAX_LEVEL, LED_MAX_LEVEL, 0 }
};

static auto& lcd = M5.Display;
static M5Canvas frame(&M5.Display);

static coop::RaidEngine game;
static motion::MotionController motionCtrl;
static acoustic::CombatToneDecoder toneDecoder;

static int myPokemon = 1;      // Default Charmander
static uint16_t myId = 0x1001;
static uint16_t partnerId = 0x1002;
static int partnerPokemon = 0; // Default Bulbasaur

enum class AudioState { Listening, Transmitting, Idle };
static AudioState audioState = AudioState::Idle;

static int16_t micBuffer[acoustic::BLOCK];
static bool micPending = false;
static int sendStep = 0;
static uint32_t nextSendStep = 0;
static acoustic::CombatAction pendingSendAction = acoustic::CombatAction::Unknown;

static uint32_t nextSpriteFrame = 0, nextEnemySpriteFrame = 0;
static uint16_t spriteIndex = 0, enemySpriteIndex = 0;
static String shoutBanner = "¡Prepárense para la Incursión!";
static uint32_t shoutBannerUntil = 0;

static void text(const String& s, int x, int y, uint16_t color = TFT_WHITE) {
    lcd.setTextColor(color, BG);
    lcd.drawString(s, x, y);
}

static void drawHealthBar(int x, int y, int w, int h, int current, int max, uint16_t color) {
    lcd.fillRoundRect(x, y, w, h, 3, 0x2188);
    if (current > 0 && max > 0) {
        int fillW = (w * current) / max;
        fillW = std::min(w, std::max(1, fillW));
        lcd.fillRoundRect(x, y, fillW, h, 3, color);
    }
}

static void drawUi() {
    lcd.fillScreen(BG);

    // Top: Raid Boss Status Bar
    text(String("JEFE: ") + game.boss.name, 10, 8, TFT_RED);
    char bossHpStr[32];
    snprintf(bossHpStr, sizeof(bossHpStr), "%d / %d PS", game.boss.hp, game.boss.maxHp);
    text(bossHpStr, 220, 8, MUTED);
    drawHealthBar(10, 26, 300, 10, game.boss.hp, game.boss.maxHp, TFT_RED);

    // Players info & health
    text(String(NAMES[myPokemon]) + " (TÚ)", 10, 42, COLORS[myPokemon]);
    drawHealthBar(10, 58, 140, 8, game.players[0].hp, game.players[0].maxHp, COLORS[myPokemon]);

    if (game.playerCount > 1) {
        text(String(NAMES[partnerPokemon]) + " (SOCIO)", 170, 42, COLORS[partnerPokemon]);
        drawHealthBar(170, 58, 140, 8, game.players[1].hp, game.players[1].maxHp, COLORS[partnerPokemon]);
    } else {
        text("Esperando compañero...", 170, 42, MUTED);
    }

    // Shout / Dialogue Banner
    lcd.fillRect(0, 178, 320, 24, BG);
    text(shoutBanner, 10, 180, TFT_YELLOW);

    // Footer buttons
    lcd.fillRoundRect(8, 206, 148, 28, 6, 0x2188);
    lcd.fillRoundRect(164, 206, 148, 28, 6, 0x2188);
    text(game.state == coop::BattleState::Fighting ? "Batalla Activa" : "Iniciar Incursión", 16, 212);
    text(audioState == AudioState::Listening ? "Micro: Escuchando" : "Micro: Standby", 172, 212, TFT_CYAN);
}

static void stopAudio() {
    I2CGuard lock;
    if (M5.Mic.isRunning()) M5.Mic.end();
    micPending = false;
    if (M5.Speaker.isRunning()) { M5.Speaker.stop(); M5.Speaker.end(); }
    audioState = AudioState::Idle;
}

static void startListening() {
    stopAudio();
    {
        I2CGuard lock;
        if (!M5.Mic.begin()) {
            shoutBanner = "Error iniciando micrófono";
            return;
        }
    }
    toneDecoder.reset();
    audioState = AudioState::Listening;
}

static void emitCombatAction(acoustic::CombatAction act) {
    stopAudio();
    {
        I2CGuard lock;
        if (!M5.Speaker.begin()) return;
        M5.Speaker.setVolume(140);
    }
    pendingSendAction = act;
    audioState = AudioState::Transmitting;
    sendStep = 0;
    nextSendStep = millis();
}

static acoustic::CombatAction mapActionToCombatTone(coop::ActionType act, coop::Element elem) {
    if (act == coop::ActionType::Heal) return acoustic::CombatAction::HealAlly;
    if (act == coop::ActionType::Shield) return acoustic::CombatAction::ShieldTeam;
    if (act == coop::ActionType::ComboUlt) return acoustic::CombatAction::DuoUltimate;
    if (elem == coop::Element::Fire) return acoustic::CombatAction::AttackFire;
    if (elem == coop::Element::Water) return acoustic::CombatAction::AttackWater;
    return acoustic::CombatAction::AttackGrass;
}

static void handleReceivedAction(acoustic::CombatAction act, uint32_t now) {
    if (act == acoustic::CombatAction::AttackWater || act == acoustic::CombatAction::AttackFire || act == acoustic::CombatAction::AttackGrass) {
        coop::Element elem = (act == acoustic::CombatAction::AttackFire) ? coop::Element::Fire :
                             (act == acoustic::CombatAction::AttackWater) ? coop::Element::Water : coop::Element::Grass;
        game.executePlayerAction(partnerId, coop::ActionType::Attack, now);
        motionCtrl.takeDamage(now); // visual recoil as the arena shakes
    } else if (act == acoustic::CombatAction::HealAlly) {
        game.executePlayerAction(partnerId, coop::ActionType::Heal, now);
        motionCtrl.victory(now);
    } else if (act == acoustic::CombatAction::ShieldTeam) {
        game.executePlayerAction(partnerId, coop::ActionType::Shield, now);
    } else if (act == acoustic::CombatAction::DuoUltimate) {
        game.executePlayerAction(partnerId, coop::ActionType::ComboUlt, now);
        motionCtrl.attack(now);
    }

    shoutBanner = String("Compañero: ") + acoustic::actionDescription(act);
    shoutBannerUntil = now + 2500;
    drawUi();
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    Serial.setTxTimeoutMs(0);

    lcd.setRotation(1);
    lcd.setBrightness(170);
    lcd.setFont(&fonts::efontJA_16);
    lcd.setTextDatum(top_left);
    lcd.setTextWrap(false);

    frame.setPsram(true);
    frame.setColorDepth(16);
    frame.createSprite(140, 110);

    boardInit(true);
    servoInit();
    delay(400);

    // Initialize Autonomous Raid Engine
    game.initRaid(1);
    game.addPlayer(myId, myPokemon, 3);
    game.addPlayer(partnerId, partnerPokemon, 2);
    game.startBattle(millis());

    setBaseLeds(LEDS[myPokemon][0], LEDS[myPokemon][1], LEDS[myPokemon][2]);
    drawUi();
    startListening();

    Serial.println("[coop-raid] Autonomous Raid Engine started. Listening for tones...");
}

void loop() {
    {
        I2CGuard lock;
        M5.update();
    }
    const uint32_t now = millis();
    motionCtrl.update(now);

    // Touch interactions
    const auto touch = M5.Touch.getDetail();
    if (touch.wasClicked()) {
        if (touch.y < 50 && touch.x < 150) {
            // Cycle my pokemon
            myPokemon = (myPokemon + 1) % 3;
            game.players[0].pokemon = myPokemon;
            game.players[0].element = static_cast<coop::Element>(myPokemon % 4);
            setBaseLeds(LEDS[myPokemon][0], LEDS[myPokemon][1], LEDS[myPokemon][2]);
            drawUi();
        } else if (touch.y >= 200 && touch.x < 160) {
            // Toggle start/stop
            if (game.state != coop::BattleState::Fighting) {
                game.initRaid(1);
                game.addPlayer(myId, myPokemon, 3);
                game.addPlayer(partnerId, partnerPokemon, 2);
                game.startBattle(now);
            }
            drawUi();
        }
    }

    // Audio transmission step machine
    if (audioState == AudioState::Transmitting && (int32_t)(now - nextSendStep) >= 0) {
        if (sendStep >= 6) {
            M5.Speaker.stop();
            startListening(); // Resume listening after speech
        } else if (!(sendStep & 1)) {
            int toneHz = (sendStep == 0) ? acoustic::FREQUENCIES[acoustic::TONE_START] :
                         (sendStep == 2) ? acoustic::FREQUENCIES[actionToTone(pendingSendAction)] :
                                           acoustic::FREQUENCIES[acoustic::TONE_END];
            M5.Speaker.tone(toneHz, acoustic::NOTE_MS);
            nextSendStep = now + acoustic::NOTE_MS;
            ++sendStep;
        } else {
            M5.Speaker.stop();
            nextSendStep = now + acoustic::GAP_MS;
            ++sendStep;
        }
    }

    // Audio listening and tone decoding
    if (audioState == AudioState::Listening) {
        if (micPending && !M5.Mic.isRecording()) {
            const int decodedTone = toneDecoder.consume(micBuffer, now);
            micPending = false;
            if (decodedTone >= 0) {
                acoustic::CombatAction act = acoustic::toneToAction(decodedTone);
                handleReceivedAction(act, now);
            }
        }
        if (!micPending && audioState == AudioState::Listening) {
            micPending = M5.Mic.record(micBuffer, acoustic::BLOCK, acoustic::RATE);
        }
    }

    // Autonomous Decision Loop for Robot 1 (Myself)
    if (game.state == coop::BattleState::Fighting && audioState != AudioState::Transmitting) {
        coop::PlayerState& self = game.players[0];
        if (self.isAlive() && now >= self.nextActionTime) {
            coop::AutonomousDecision dec = game.decideAutonomousAction(myId, now);
            game.executePlayerAction(myId, dec.action, now, dec.shout);

            // Trigger physical servo animation
            if (dec.action == coop::ActionType::Attack || dec.action == coop::ActionType::ComboUlt) {
                motionCtrl.attack(now);
            } else if (dec.action == coop::ActionType::Shield) {
                motionCtrl.charge(now);
            } else if (dec.action == coop::ActionType::Heal) {
                motionCtrl.victory(now);
            }

            shoutBanner = String("Tú: ") + dec.shout;
            shoutBannerUntil = now + 2500;
            drawUi();

            // Broadcast decision over acoustic tones
            acoustic::CombatAction act = mapActionToCombatTone(dec.action, self.element);
            emitCombatAction(act);
        }
    }

    // Tick raid engine (boss AI turn, combo expiry, etc.)
    game.tick(now);
    if (game.dirty) {
        game.dirty = false;
        drawUi();
    }

    if (shoutBannerUntil && now > shoutBannerUntil) {
        shoutBannerUntil = 0;
        shoutBanner = "En guardia...";
        drawUi();
    }

    delay(2);
}
