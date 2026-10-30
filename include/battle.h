#pragma once
#include <stdint.h>

// No Arduino dependencies: this is also the engine exercised by native tests.
namespace battle {
enum class State { Idle, Searching, Fighting, Finished, LinkLost };
enum class Kind : uint8_t { Hello, Ack, Sync, Bye };
struct Packet { Kind kind; uint16_t id; uint8_t pokemon; uint8_t round; };

inline uint32_t encode(const Packet& p) {
    const uint32_t body = (p.kind==Kind::Hello || p.kind==Kind::Ack)
        ? ((uint32_t(p.pokemon)&3)<<16) | p.id
        : ((uint32_t(p.id)&0xFFF)<<6) | (p.round&63);
    const uint32_t data = 0xD00000 | (uint32_t(p.kind)<<18) | body;
    // Extended NEC: 16 address bits, command byte and its complement.
    return (data<<8) | ((data&255)^255);
}
inline bool decode(uint64_t raw, Packet& p) {
    if (raw>UINT32_MAX || ((raw>>28)&15)!=0xD || ((raw&255)^((raw>>8)&255))!=255) return false;
    const uint32_t data=raw>>8;
    p.kind=Kind((data>>18)&3);
    if (p.kind==Kind::Hello || p.kind==Kind::Ack) {
        p.id=data&65535; p.pokemon=(data>>16)&3; p.round=0;
        return p.id!=0 && p.pokemon<3;
    }
    p.id=(data>>6)&0xFFF; p.round=data&63; p.pokemon=0;
    return true;
}
inline uint32_t mix(uint32_t x) { x^=x<<13; x^=x>>17; x^=x<<5; return x; }
inline uint16_t sessionId(uint16_t low, uint16_t high, uint8_t a, uint8_t b) {
    return mix(((uint32_t(low)<<16)|high) ^ (0x9E3779B9u+uint32_t(a)*31+uint32_t(b)*131))&0xFFF;
}
inline int damage(uint8_t attacker, uint8_t defender, uint32_t seed, uint8_t round) {
    int n=12+mix(seed ^ (uint32_t(round)*0x9E3779B9u))%7;
    // Index: grass, fire, water. Fire beats grass, water beats fire, grass beats water.
    if ((attacker+2)%3==defender) return n*2;
    if ((attacker+1)%3==defender) return n/2;
    return n;
}

class Engine {
public:
    State state=State::Idle;
    uint16_t token=0, peer=0, session=0;
    uint8_t pokemon=0, opponent=0, round=0, peerRound=0;
    int hp[2]={100,100}; // canonical order: lower token, higher token
    int lastDamage=0, lastAttacker=-1;
    bool dirty=true;

    int mySide() const { return token<peer ? 0 : 1; }
    int myHp() const { return hp[mySide()]; }
    int theirHp() const { return hp[1-mySide()]; }
    bool won() const { return state==State::Finished && myHp()>0; }
    bool paired() const { return peer!=0; }
    void choose(uint16_t id, uint8_t mon, uint32_t now) {
        token=id ? id : 1; pokemon=mon<3 ? mon : 0; peer=0; session=0;
        hp[0]=hp[1]=100; round=peerRound=0; lastAttacker=-1; lastDamage=0;
        state=State::Searching; lastContact=lastProgress=now; helloTurn=false; dirty=true;
    }
    void stop() { state=State::Idle; peer=0; dirty=true; }
    void receive(const Packet& p, uint32_t now) {
        if (state==State::Idle || state==State::LinkLost) return;
        if (p.kind==Kind::Hello) {
            if (state!=State::Searching || p.id==token || p.pokemon>2) return;
            if (peer && (peer!=p.id || opponent!=p.pokemon)) return;
            peer=p.id; opponent=p.pokemon; updateSession(); lastContact=now; dirty=true;
        } else if (p.kind==Kind::Ack) {
            if (state==State::Searching && peer && p.id==token && p.pokemon==opponent) start(now);
        } else {
            if (!peer || p.id!=session) return;
            if (p.kind==Kind::Bye) { state=State::LinkLost; dirty=true; return; }
            if (p.round>round+1) return; // a peer may be at most one confirmed turn ahead
            if (state==State::Searching && p.round<=1) start(now);
            if (state!=State::Fighting && state!=State::Finished) return;
            lastContact=now;
            if (p.round>peerRound) peerRound=p.round;
        }
    }
    bool transmit(Packet& p) {
        if (state==State::Searching) {
            helloTurn=!helloTurn;
            if (peer && helloTurn) p={Kind::Ack,peer,pokemon,0};
            else p={Kind::Hello,token,pokemon,0};
            return true;
        }
        if (state==State::Fighting || state==State::Finished) {
            p={Kind::Sync,session,0,round}; return true;
        }
        return false;
    }
    void tick(uint32_t now) {
        if (state==State::Searching) {
            if (peer && now-lastContact>12000) { peer=0; dirty=true; }
            return;
        }
        if (state!=State::Fighting) return;
        if (now-lastContact>12000 || now-lastProgress>20000) {
            state=State::LinkLost; dirty=true; return;
        }
        if (peerRound<round || now-lastProgress<1800) return;
        const int attacker=round%2, defender=1-attacker;
        const uint8_t a=mySide()==0 ? pokemon : opponent;
        const uint8_t b=mySide()==0 ? opponent : pokemon;
        const uint32_t seed=(uint32_t(token<peer ? token : peer)<<16)|(token<peer ? peer : token);
        lastDamage=damage(attacker==0 ? a : b,defender==0 ? a : b,seed,round);
        lastAttacker=attacker;
        hp[defender]=hp[defender]>lastDamage ? hp[defender]-lastDamage : 0;
        ++round; lastProgress=now; dirty=true;
        if (!hp[defender]) state=State::Finished;
    }
private:
    uint32_t lastContact=0,lastProgress=0;
    bool helloTurn=false;
    void start(uint32_t now) { state=State::Fighting; lastContact=lastProgress=now; dirty=true; }
    void updateSession() {
        session=token<peer ? sessionId(token,peer,pokemon,opponent) : sessionId(peer,token,opponent,pokemon);
    }
};
} // namespace battle
