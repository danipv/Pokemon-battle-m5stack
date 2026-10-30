#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
namespace tones {
static constexpr uint32_t RATE=16000;
static constexpr size_t BLOCK=320;
struct Bands {int tone=-1;float rms=0,confidence=0;};
template<size_t N> Bands classify(const int16_t* samples,const int (&frequencies)[N]) {
    float mean=0,energy=0;
    for(size_t i=0;i<BLOCK;++i) mean+=samples[i];
    mean/=BLOCK;
    for(size_t i=0;i<BLOCK;++i) {const float x=samples[i]-mean;energy+=x*x;}
    float best=0,second=0;int winner=-1;
    for(size_t f=0;f<N;++f) {
        const float coefficient=2*cosf(6.28318530718f*frequencies[f]/RATE);
        float a=0,b=0;
        for(size_t i=0;i<BLOCK;++i) {const float v=samples[i]-mean+coefficient*a-b;b=a;a=v;}
        const float power=a*a+b*b-coefficient*a*b;
        if(power>best) {second=best;best=power;winner=f;} else if(power>second) second=power;
    }
    Bands result;result.rms=sqrtf(energy/BLOCK);
    result.confidence=energy>0 ? 2*best/(BLOCK*energy) : 0;
    if(result.rms>=50 && result.confidence>=0.40f && best>second*2.5f) result.tone=winner;
    return result;
}
}
