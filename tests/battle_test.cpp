#include <assert.h>
#include <stdio.h>
#include "battle.h"
using namespace battle;

static Packet wire(const Packet& p) { Packet q{}; assert(decode(encode(p),q)); return q; }
static void exchange(Engine& a, Engine& b, uint32_t now) {
    Packet p{};
    if (a.transmit(p)) b.receive(wire(p),now);
    if (b.transmit(p)) a.receive(wire(p),now);
}
static void testMatch(int aMon,int bMon,uint32_t random) {
    Engine a,b;
    a.choose(120,aMon,0); b.choose(700,bMon,0);
    Packet delayedA{},delayedB{}; bool haveA=false,haveB=false;
    for (uint32_t now=0;now<180000;now+=200) {
        Packet p{};
        // Lost frames, duplicates and packets delivered one interval late.
        random=mix(random);
        if (haveA && (random&1)) { b.receive(delayedA,now); haveA=false; }
        if (haveB && (random&2)) { a.receive(delayedB,now); haveB=false; }
        if (a.transmit(p)) {
            p=wire(p);
            if (random%4) { b.receive(p,now); if(random&16) b.receive(p,now); }
            else {delayedA=p; haveA=true;}
        }
        random=mix(random);
        if (b.transmit(p)) {
            p=wire(p);
            if (random%4) {a.receive(p,now); if(random&32) a.receive(p,now);}
            else {delayedB=p; haveB=true;}
        }
        a.tick(now); b.tick(now);
        assert(a.round<=b.round+1 && b.round<=a.round+1);
        assert(a.state!=State::LinkLost && b.state!=State::LinkLost);
        if (a.state==State::Finished && b.state==State::Finished) {
            assert(a.round==b.round);
            assert(a.hp[0]==b.hp[0] && a.hp[1]==b.hp[1]);
            assert(a.won()!=b.won());
            assert(a.session==b.session);
            return;
        }
    }
    assert(!"match failed to converge");
}
int main() {
    for (uint32_t id=1;id<=65535;++id) for(int mon=0;mon<3;++mon) {
        for (int k=0;k<2;++k) {
            const Kind kind=Kind(k);
            Packet p{kind,uint16_t(id),uint8_t(mon),0},q=wire(p);
            assert(q.id==id && q.pokemon==mon && q.kind==kind);
        }
    }
    for (uint16_t tag=0;tag<4096;++tag) for(int r=0;r<64;++r) {
        Packet p{Kind::Sync,tag,0,uint8_t(r)},q=wire(p);
        assert(q.id==tag && q.round==r);
    }
    Packet q{};
    assert(!decode(0,q)); assert(!decode(0x12345678,q));
    assert(!decode(encode({Kind::Hello,42,3,0}),q));
    assert(!decode(encode({Kind::Hello,42,0,0})^1,q));
    assert(damage(0,2,123,0)>damage(0,1,123,0));
    for(int a=0;a<3;++a) for(int b=0;b<3;++b) for(uint32_t seed=1;seed<=100;++seed)
        testMatch(a,b,seed);
    Engine a,b; a.choose(20,0,0); b.choose(80,1,0);
    for (uint32_t now=0;now<2000;now+=200) exchange(a,b,now);
    assert(a.state==State::Fighting && b.state==State::Fighting);
    const int old=a.hp[0]+a.hp[1];
    a.receive({Kind::Sync,uint16_t(a.session^1),0,1},2100);
    a.receive({Kind::Sync,a.session,0,63},2100);
    assert(a.peerRound==0 && old==a.hp[0]+a.hp[1]);
    a.tick(15000); assert(a.state==State::LinkLost);
    b.receive({Kind::Bye,b.session,0,b.round},2000); assert(b.state==State::LinkLost);
    a.choose(33,2,17000); assert(a.state==State::Searching && a.hp[0]==100 && !a.paired());
    a.receive({Kind::Hello,33,0,0},17000); assert(!a.paired()); // reflected/equal nonce
    a.stop(); a.receive({Kind::Hello,99,0,0},18000); assert(a.state==State::Idle);
    puts("PASS: packet roundtrips, 900 lossy matches, duplicates, delays, type damage, timeouts and restart");
}
