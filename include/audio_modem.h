#pragma once
#include "battle.h"
namespace sound {
enum class Mode { Idle,Listening,Sending,Error };
struct Status {
    Mode mode=Mode::Idle;
    float rms=0,confidence=0;
    int toneHz=0,pokemon=-1;
    uint32_t received=0,sent=0,rejected=0,heartbeat=0,lastSignal=0;
    bool inFrame=false;
};
bool begin();
void listenTest();
void listenPackets();
void stop();
bool sendPokemon(unsigned pokemon);
bool sendPacket(const battle::Packet& packet);
Status status();
bool takePacket(battle::Packet& packet);
}
