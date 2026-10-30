#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>

namespace tones {
static constexpr uint32_t RATE=16000;
static constexpr size_t BLOCK=320;
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
        float mean=0;
        for(size_t i=0;i<BLOCK;++i) mean+=samples[i];
        mean/=BLOCK;
        float energy=0;
        for(size_t i=0;i<BLOCK;++i) { const float x=samples[i]-mean; energy+=x*x; }
        rms=sqrtf(energy/BLOCK);
        float best=0,second=0;
        int winner=-1;
        for(int f=0;f<5;++f) {
            const float coefficient=2*cosf(6.28318530718f*FREQUENCIES[f]/RATE);
            float a=0,b=0;
            for(size_t i=0;i<BLOCK;++i) {
                const float current=samples[i]-mean+coefficient*a-b;
                b=a; a=current;
            }
            const float power=a*a+b*b-coefficient*a*b;
            if(power>best) {second=best;best=power;winner=f;} else if(power>second) second=power;
        }
        confidence=energy>0 ? 2*best/(BLOCK*energy) : 0;
        tone=rms>=50 && confidence>=0.40f && best>second*2.5f ? winner : -1;
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
