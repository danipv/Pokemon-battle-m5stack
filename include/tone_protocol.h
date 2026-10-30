#pragma once
#include <stdint.h>
#include <stddef.h>
#include "tone_bands.h"

namespace tones {
static constexpr int FREQUENCIES[]={1000,1400,1800,2400,2800};
static constexpr unsigned NOTE_MS=300,GAP_MS=100;
// Frame: 2400 Hz start, 1000/1400/1800 Hz Pokemon ID, 2800 Hz end.
// A diagnostic transport v0.1, not yet a reliable battle packet protocol.
class Decoder {
public:
    int tone=-1;
    float rms=0,confidence=0;
    void reset() { state=0; candidate=-1; stable=0; emitted=-1; payload=-1; tone=-1; }
    int consume(const int16_t* samples,uint32_t now) {
        const auto detected=classify(samples,FREQUENCIES);
        rms=detected.rms;confidence=detected.confidence;tone=detected.tone;
        if(state && now-started>2000) {state=0;payload=-1;}
        if(tone!=candidate) {candidate=tone;stable=1;} else if(stable<255) ++stable;
        if(tone<0) {if(stable>=2) emitted=-1;return -1;}
        if(stable<3 || emitted==tone) return -1; // 60 ms minimum, one event per note
        emitted=tone;
        if(tone==3) {state=1;started=now;payload=-1;return -1;}
        if(state==1 && tone<3) {payload=tone;state=2;return -1;}
        if(state==2 && tone==4) {const int result=payload;state=0;payload=-1;return result;}
        state=0;payload=-1;
        return -1;
    }
private:
    uint8_t state=0,stable=0;
    int candidate=-1,emitted=-1,payload=-1;
    uint32_t started=0;
};
}
