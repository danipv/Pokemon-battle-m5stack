#pragma once
#include <stdint.h>

#ifdef ARDUINO
#include "board.h"
#include <M5Unified.h>
#else
// Stub for native unit tests
inline void servoSetTarget(float, float) {}
inline void servoHome() {}
inline void setBaseLeds(uint8_t, uint8_t, uint8_t) {}
#endif

namespace motion {

enum class Pose : uint8_t {
    Neutral = 0,
    LungeAttack,
    RecoilHurt,
    ChargePower,
    CelebrateVictory,
    FaintDefeat
};

class MotionController {
public:
    Pose currentPose = Pose::Neutral;
    uint32_t poseEndTime = 0;
    bool isAnimating = false;

    void update(uint32_t now) {
        if (!isAnimating) return;
        if (now >= poseEndTime) {
            isAnimating = false;
            currentPose = Pose::Neutral;
            servoSetTarget(0.0f, 0.0f); // return to center
        }
    }

    // Quick physical lunge forward when launching an attack
    void attack(uint32_t now) {
        currentPose = Pose::LungeAttack;
        // Tilt forward (positive tilt = pitch down in SCS0009 setup)
        servoSetTarget(0.0f, 0.25f);
        poseEndTime = now + 450;
        isAnimating = true;
    }

    // Head jerks back and wobbles when taking a hit
    void takeDamage(uint32_t now) {
        currentPose = Pose::RecoilHurt;
        // Tilt up/back and slight pan recoil
        servoSetTarget(-0.18f, -0.22f);
        poseEndTime = now + 500;
        isAnimating = true;
    }

    // Tilt head back while charging an ultimate or combo
    void charge(uint32_t now) {
        currentPose = Pose::ChargePower;
        servoSetTarget(0.0f, -0.30f);
        poseEndTime = now + 900;
        isAnimating = true;
    }

    // Cheerful nodding and celebratory head wiggle
    void victory(uint32_t now) {
        currentPose = Pose::CelebrateVictory;
        servoSetTarget(0.20f, 0.15f);
        poseEndTime = now + 1600;
        isAnimating = true;
    }

    // Faint down when HP reaches 0
    void faint(uint32_t now) {
        currentPose = Pose::FaintDefeat;
        // Head hangs completely down
        servoSetTarget(0.0f, 0.40f);
        poseEndTime = now + 10000;
        isAnimating = true;
    }
};

} // namespace motion
