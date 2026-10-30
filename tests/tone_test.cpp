#include <assert.h>
#include <stdio.h>
#include "tone_protocol.h"

static uint32_t now=0,randomState=1;
static int feed(tones::Decoder& d,int hz,int frames,int amplitude=3000,int noise=0,int dc=0) {
    int result=-1;
    for(int frame=0;frame<frames;++frame) {
        int16_t samples[tones::BLOCK];
        for(size_t i=0;i<tones::BLOCK;++i) {
            randomState=randomState*1664525u+1013904223u;
            const int n=noise ? int(randomState%(2*noise+1))-noise : 0;
            samples[i]=int16_t(dc+n+(hz ? amplitude*sinf(6.28318530718f*hz*i/tones::RATE) : 0));
        }
        now+=20;
        const int found=d.consume(samples,now);
        if(found>=0) {assert(result<0);result=found;}
    }
    return result;
}
int main() {
    for(int mon=0;mon<3;++mon) for(int level=0;level<3;++level) {
        const int noise=level==0 ? 0 : level==1 ? 400 : 1200;
        tones::Decoder d;now=0;
        assert(feed(d,0,10,3000,noise,1200)<0);
        assert(feed(d,2400,15,3000,noise,1200)<0);
        feed(d,0,5,3000,noise,1200);
        assert(feed(d,tones::FREQUENCIES[mon],15,3000,noise,1200)<0);
        feed(d,0,5,3000,noise,1200);
        assert(feed(d,2800,15,3000,noise,1200)==mon);
        assert(feed(d,2800,30)<0); // no duplicate result from a sustained end note
    }
    tones::Decoder d;now=0;
    assert(feed(d,1000,15)<0);assert(feed(d,2800,15)<0); // no header
    d.reset();feed(d,2400,15);assert(feed(d,2800,15)<0); // no payload
    d.reset();feed(d,2400,15);feed(d,1000,15);feed(d,0,120);
    assert(feed(d,2800,15)<0); // timeout
    d.reset();assert(feed(d,2400,2)<0);feed(d,0,5);feed(d,1000,15);
    assert(feed(d,2800,15)<0); // too short header
    d.reset();assert(feed(d,2400,15,10)<0); // below microphone floor
    assert(feed(d,900,15)<0); // off-frequency signal
    puts("PASS: all three tone sequences, noise/DC, no duplicates, missing notes, timeout, short/weak/off-frequency signals");
}
