#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>

namespace acoustic {

static constexpr uint32_t RATE = 16000;
static constexpr size_t BLOCK = 320; // 20 ms blocks
static constexpr unsigned NOTE_MS = 250; // duration of each tone note
static constexpr unsigned GAP_MS = 80;   // gap between tone notes

// 8 orthogonal frequencies (all multiples of 200 Hz, resolution is 50 Hz)
enum ToneCode : int8_t {
    TONE_NONE = -1,
    TONE_ATK_GRASS = 0,    // 1000 Hz
    TONE_HEAL = 1,         // 1200 Hz
    TONE_ATK_FIRE = 2,     // 1400 Hz
    TONE_SHIELD = 3,       // 1600 Hz
    TONE_ATK_WATER = 4,    // 1800 Hz
    TONE_ULTIMATE = 5,     // 2000 Hz
    TONE_START = 6,        // 2400 Hz
    TONE_END = 7           // 2800 Hz
};

static constexpr int FREQUENCIES[] = {
    1000, // 0: Grass Attack
    1200, // 1: Heal / Support
    1400, // 2: Fire Attack
    1600, // 3: Shield / Defense
    1800, // 4: Water Attack
    2000, // 5: Duo Ultimate
    2400, // 6: Start Header
    2800  // 7: End Footer
};
static constexpr size_t FREQ_COUNT = sizeof(FREQUENCIES) / sizeof(FREQUENCIES[0]);

enum class CombatAction : uint8_t {
    AttackGrass = 0,
    HealAlly = 1,
    AttackFire = 2,
    ShieldTeam = 3,
    AttackWater = 4,
    DuoUltimate = 5,
    Unknown = 255
};

inline const char* actionDescription(CombatAction act) {
    switch (act) {
        case CombatAction::AttackGrass: return "¡Ataque Planta (Látigo Cepa)!";
        case CombatAction::HealAlly:    return "¡Cura de Apoyo al Compañero!";
        case CombatAction::AttackFire:  return "¡Ataque Fuego (Lanzallamas)!";
        case CombatAction::ShieldTeam:  return "¡Escudo Protector de Equipo!";
        case CombatAction::AttackWater: return "¡Ataque Agua (Pistola Agua)!";
        case CombatAction::DuoUltimate: return "¡Ataque Definitivo en Pareja!";
        default: return "Acción Desconocida";
    }
}

inline CombatAction toneToAction(int toneIndex) {
    if (toneIndex >= 0 && toneIndex <= 5) {
        return static_cast<CombatAction>(toneIndex);
    }
    return CombatAction::Unknown;
}

inline int actionToTone(CombatAction act) {
    return static_cast<int>(act);
}

// Multi-tone Goertzel detector tuned for acoustic battle actions
class CombatToneDecoder {
public:
    int detectedTone = TONE_NONE;
    float rms = 0.0f;
    float confidence = 0.0f;

    void reset() {
        state = 0; candidate = -1; stable = 0; emitted = -1;
        payloadTone = -1; detectedTone = TONE_NONE;
    }

    // Returns a CombatAction when a full 3-tone chord sequence [START -> ACTION -> END] is decoded
    int consume(const int16_t* samples, uint32_t now) {
        float mean = 0.0f;
        for (size_t i = 0; i < BLOCK; ++i) mean += samples[i];
        mean /= BLOCK;

        float energy = 0.0f;
        for (size_t i = 0; i < BLOCK; ++i) {
            const float x = samples[i] - mean;
            energy += x * x;
        }
        rms = sqrtf(energy / BLOCK);

        float best = 0.0f, second = 0.0f;
        int winner = -1;

        for (size_t f = 0; f < FREQ_COUNT; ++f) {
            const float coef = 2.0f * cosf(6.28318530718f * FREQUENCIES[f] / RATE);
            float a = 0.0f, b = 0.0f;
            for (size_t i = 0; i < BLOCK; ++i) {
                const float current = samples[i] - mean + coef * a - b;
                b = a; a = current;
            }
            const float power = a * a + b * b - coef * a * b;
            if (power > best) {
                second = best; best = power; winner = static_cast<int>(f);
            } else if (power > second) {
                second = power;
            }
        }

        confidence = energy > 0 ? 2.0f * best / (BLOCK * energy) : 0.0f;
        detectedTone = (rms >= 45.0f && confidence >= 0.38f && best > second * 2.2f) ? winner : TONE_NONE;

        // Timeout 2 seconds
        if (state && now - started > 2000) {
            state = 0; payloadTone = -1;
        }

        // Stability filtering: requires at least 2 consecutive frames (~40 ms)
        if (detectedTone != candidate) {
            candidate = detectedTone;
            stable = 1;
        } else if (stable < 255) {
            ++stable;
        }

        if (detectedTone < 0) {
            if (stable >= 2) emitted = -1;
            return -1;
        }

        if (stable < 2 || emitted == detectedTone) return -1;
        emitted = detectedTone;

        // State Machine: [START (2400 Hz)] -> [PAYLOAD (1000..2000 Hz)] -> [END (2800 Hz)]
        if (detectedTone == TONE_START) {
            state = 1;
            started = now;
            payloadTone = -1;
            return -1;
        }
        if (state == 1 && detectedTone <= 5) {
            payloadTone = detectedTone;
            state = 2;
            return -1;
        }
        if (state == 2 && detectedTone == TONE_END) {
            const int result = payloadTone;
            state = 0;
            payloadTone = -1;
            return result; // Successfully decoded action!
        }

        state = 0;
        payloadTone = -1;
        return -1;
    }

private:
    uint8_t state = 0;
    uint8_t stable = 0;
    int candidate = -1;
    int emitted = -1;
    int payloadTone = -1;
    uint32_t started = 0;
};

} // namespace acoustic
