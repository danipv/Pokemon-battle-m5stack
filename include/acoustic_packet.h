#pragma once
#include "battle.h"
#include "tone_bands.h"
namespace acoustic {
static constexpr int FREQUENCIES[]={1000,1400,1800,2200,2600,3000};
static constexpr uint32_t FRAME_MS=4700;
static constexpr unsigned SYMBOLS=16;
struct Note {uint16_t hz,ms;};
static constexpr unsigned NOTES=36;
inline uint8_t crc8(uint32_t payload) {
    uint8_t crc=0;
    for(int shift=16;shift>=0;shift-=8) {
        crc^=uint8_t(payload>>shift);
        for(int bit=0;bit<8;++bit) crc=(crc&128) ? uint8_t((crc<<1)^0x07) : uint8_t(crc<<1);
    }
    return crc;
}
inline uint32_t encode(const battle::Packet& p) {
    const uint32_t payload=battle::encode(p)>>8;
    return (payload<<8)|crc8(payload);
}
inline bool decode(uint32_t wire,battle::Packet& p) {
    const uint32_t payload=wire>>8;
    if(uint8_t(wire)!=crc8(payload)) return false;
    return battle::decode((payload<<8)|((payload&255)^255),p);
}
inline void notes(const battle::Packet& p,Note (&out)[NOTES]) {
    const uint32_t wire=acoustic::encode(p);
    out[0]={2600,300};out[1]={0,100};
    for(unsigned i=0;i<SYMBOLS;++i) {
        out[2+2*i]={uint16_t(FREQUENCIES[(wire>>(30-2*i))&3]),160};
        out[3+2*i]={0,80};
    }
    out[34]={3000,300};out[35]={0,160};
}
class Decoder {
public:
    tones::Bands signal;
    uint32_t rejected=0;
    bool inFrame() const {return active;}
    void reset() {active=false;count=0;candidate=-1;emitted=-1;stable=0;wire=0;}
    bool consume(const int16_t* samples,uint32_t now,battle::Packet& p) {
        signal=tones::classify(samples,FREQUENCIES);
        if(active && now-started>7000) {active=false;++rejected;}
        const int tone=signal.tone;
        if(tone!=candidate) {candidate=tone;stable=1;} else if(stable<255) ++stable;
        if(tone<0) {if(stable>=2) emitted=-1;return false;}
        if(stable<3 || tone==emitted) return false;
        emitted=tone;
        if(tone==4) {active=true;count=0;wire=0;started=now;return false;}
        if(!active) return false;
        if(tone<4 && count<SYMBOLS) {wire=(wire<<2)|unsigned(tone);++count;return false;}
        active=false;
        if(tone==5 && count==SYMBOLS && decode(wire,p)) return true;
        ++rejected;return false;
    }
private:
    bool active=false;
    uint8_t count=0,stable=0;
    int candidate=-1,emitted=-1;
    uint32_t started=0,wire=0;
};
}
