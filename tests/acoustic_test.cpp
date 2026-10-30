#include <assert.h>
#include <stdio.h>
#include "acoustic_packet.h"
using namespace battle;

static uint32_t randomState=123;
static uint32_t randomNumber() {randomState=mix(randomState);return randomState;}
static Packet wire(const Packet& p) {Packet q{};assert(acoustic::decode(acoustic::encode(p),q));return q;}

static int waveform(const Packet& packet,int noise,int offset,int corrupt=-1) {
    acoustic::Decoder decoder;
    acoustic::Note notes[acoustic::NOTES];acoustic::notes(packet,notes);
    if(corrupt>=0) notes[2+2*corrupt].hz=notes[2+2*corrupt].hz==1000 ? 1400 : 1000;
    unsigned duration=0;for(const auto& n:notes) duration+=n.ms;
    assert(duration==acoustic::FRAME_MS);
    int received=0;
    for(unsigned start=0;start<duration+100;start+=20) {
        int16_t samples[tones::BLOCK];
        for(unsigned i=0;i<tones::BLOCK;++i) {
            const int time=int(start*16+i)-offset;
            int hz=0,cursor=0;
            for(const auto& n:notes) {if(time>=cursor && time<cursor+int(n.ms*16)) {hz=n.hz;break;}cursor+=n.ms*16;}
            const int background=noise ? int(randomNumber()%(2*noise+1))-noise : 0;
            samples[i]=int16_t(1000+background+(hz ? 4500*sinf(6.28318530718f*hz*time/tones::RATE) : 0));
        }
        Packet got{};
        if(decoder.consume(samples,start+20,got)) {
            ++received;assert(acoustic::encode(got)==acoustic::encode(packet));
        }
    }
    return received;
}

static void simulatedMatch(int monA,int monB) {
    Engine a(1800,60000,120000),b(1800,60000,120000);
    a.choose(21,monA,0);b.choose(900,monB,0);
    uint32_t nextA=500,nextB=1200;
    for(uint32_t now=0;now<1800000;now+=100) {
        if(now>=nextA) {
            Packet p{};
            if(a.transmit(p)) {
                // A complete frame plus codec turnaround. Whole packets can be lost/corrupted.
                now+=acoustic::FRAME_MS+200;
                if(randomNumber()%5) {b.receive(wire(p),now);nextB=now+800;}
                nextA=now+1800+randomNumber()%2000;
            }
        }
        if(now>=nextB) {
            Packet p{};
            if(b.transmit(p)) {
                now+=acoustic::FRAME_MS+200;
                if(randomNumber()%5) {a.receive(wire(p),now);nextA=now+800;}
                nextB=now+1800+randomNumber()%2000;
            }
        }
        a.tick(now);b.tick(now);
        assert(a.state!=State::LinkLost && b.state!=State::LinkLost);
        if(a.state==State::Finished && b.state==State::Finished) {
            assert(a.hp[0]==b.hp[0] && a.hp[1]==b.hp[1]);assert(a.won()!=b.won());return;
        }
    }
    assert(!"acoustic battle failed to finish");
}

int main() {
    for(uint32_t id=1;id<=65535;++id) for(unsigned mon=0;mon<3;++mon) {
        Packet p{Kind::Hello,uint16_t(id),uint8_t(mon),0};
        const auto q=wire(p);assert(q.id==id && q.pokemon==mon);
    }
    for(int kind=0;kind<4;++kind) {
        Packet p{Kind(kind),uint16_t(kind<2 ? 0x1234 : 0xABC),1,12};
        const uint32_t code=acoustic::encode(p);
        for(unsigned bit=0;bit<32;++bit) {Packet q{};assert(!acoustic::decode(code^(1u<<bit),q));}
        for(int offset=0;offset<320;offset+=79) assert(waveform(p,900,offset)==1);
        assert(waveform(p,0,0,7)==0);
    }
    for(int a=0;a<3;++a) for(int b=0;b<3;++b) for(int repeat=0;repeat<10;++repeat) simulatedMatch(a,b);
    puts("PASS: acoustic CRC, all single-bit errors, timed PCM with noise/offset, corrupted symbols and 90 slow lossy battles");
}
