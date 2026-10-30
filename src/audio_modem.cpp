// This task alone owns Mic/Speaker. The UI only uses bounded queues and snapshots.
#include <M5Unified.h>
#include "board.h"
#include "audio_modem.h"
#include "tone_protocol.h"
#include "acoustic_packet.h"

namespace sound {
enum class Op { Stop,ListenTest,ListenPackets,SendTest,SendPacket };
struct Command {Op op;unsigned pokemon=0;battle::Packet packet{};};
static QueueHandle_t commands=nullptr,snapshots=nullptr,receivedPackets=nullptr;
static Status current;

static void task(void*) {
    Status s;
    tones::Decoder testDecoder;
    acoustic::Decoder packetDecoder;
    int16_t samples[tones::BLOCK];
    acoustic::Note sequence[acoustic::NOTES];
    unsigned sequenceCount=0,step=0;
    uint32_t due=0,captureStarted=0,lastPublish=0,ignoreUntil=0;
    bool packetMode=false,pending=false;
    auto publish=[&]() {s.heartbeat=millis();xQueueOverwrite(snapshots,&s);lastPublish=s.heartbeat;};
    auto closeAudio=[&]() {
        I2CGuard lock;
        if(M5.Mic.isRunning()) M5.Mic.end();
        pending=false;
        if(M5.Speaker.isRunning()) {M5.Speaker.stop();M5.Speaker.end();}
    };
    auto listen=[&]() {
        closeAudio();
        I2CGuard lock;
        auto cfg=M5.Mic.config();cfg.sample_rate=tones::RATE;M5.Mic.config(cfg);
        const bool ok=M5.Mic.begin();
        testDecoder.reset();packetDecoder.reset();s.inFrame=false;
        s.mode=ok ? Mode::Listening : Mode::Error;
        ignoreUntil=millis()+200; // discard codec startup and residual speaker echo
        return ok;
    };
    closeAudio();publish();
    for(;;) {
        Command cmd{};
        if(xQueueReceive(commands,&cmd,0)==pdTRUE) {
            if(cmd.op==Op::Stop) {closeAudio();s.mode=Mode::Idle;s.inFrame=false;}
            else if(cmd.op==Op::ListenTest || cmd.op==Op::ListenPackets) {
                packetMode=cmd.op==Op::ListenPackets;listen();
            } else {
                closeAudio();packetMode=cmd.op==Op::SendPacket;
                bool ok;
                {I2CGuard lock;ok=M5.Speaker.begin();M5.Speaker.setVolume(140);}
                if(!ok) s.mode=Mode::Error;
                else {
                    if(packetMode) {acoustic::notes(cmd.packet,sequence);sequenceCount=acoustic::NOTES;}
                    else {
                        sequence[0]={2400,300};sequence[1]={0,100};
                        sequence[2]={uint16_t(tones::FREQUENCIES[cmd.pokemon]),300};sequence[3]={0,100};
                        sequence[4]={2800,300};sequence[5]={0,100};sequenceCount=6;
                    }
                    step=0;due=millis();s.mode=Mode::Sending;
                }
            }
            publish();
        }
        const uint32_t now=millis();
        if(s.mode==Mode::Sending && (int32_t)(now-due)>=0) {
            if(step==sequenceCount) {
                M5.Speaker.stop();++s.sent;
                if(packetMode) listen();else {closeAudio();s.mode=Mode::Idle;}
                publish();
            } else {
                const auto note=sequence[step++];
                {I2CGuard lock;if(note.hz) M5.Speaker.tone(note.hz,note.ms);else M5.Speaker.stop();}
                due=millis()+note.ms;
            }
        }
        if(s.mode==Mode::Listening) {
            if(pending && !M5.Mic.isRecording()) {
                pending=false;
                if((int32_t)(now-ignoreUntil)>=0) {
                    if(packetMode) {
                        battle::Packet packet{};
                        if(packetDecoder.consume(samples,now,packet)) {
                            ++s.received;
                            xQueueSend(receivedPackets,&packet,0);
                        }
                        s.rms=packetDecoder.signal.rms;s.confidence=packetDecoder.signal.confidence;
                        s.toneHz=packetDecoder.signal.tone<0 ? 0 : acoustic::FREQUENCIES[packetDecoder.signal.tone];
                        s.inFrame=packetDecoder.inFrame();s.rejected=packetDecoder.rejected;
                    } else {
                        const int mon=testDecoder.consume(samples,now);
                        if(mon>=0) {s.pokemon=mon;++s.received;Serial.printf("[tones] RX pokemon=%d count=%lu\n",mon,(unsigned long)s.received);}
                        s.rms=testDecoder.rms;s.confidence=testDecoder.confidence;
                        s.toneHz=testDecoder.tone<0 ? 0 : tones::FREQUENCIES[testDecoder.tone];
                    }
                    if(s.toneHz) s.lastSignal=now;
                }
            }
            if(pending && now-captureStarted>400) {
                s.mode=Mode::Error;publish(); // UI sees the error before any driver shutdown
                Serial.println("[audio] capture timeout; stopping audio");
                closeAudio();
            }
            if(s.mode==Mode::Listening && !pending) {
                captureStarted=now;
                pending=M5.Mic.record(samples,tones::BLOCK,tones::RATE);
                if(!pending) {s.mode=Mode::Error;publish();closeAudio();}
            }
        }
        if(now-lastPublish>=100) publish();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

bool begin() {
    commands=xQueueCreate(1,sizeof(Command));snapshots=xQueueCreate(1,sizeof(Status));
    receivedPackets=xQueueCreate(4,sizeof(battle::Packet));
    return commands && snapshots && receivedPackets && xTaskCreatePinnedToCore(task,"audio_modem",8192,nullptr,2,nullptr,0)==pdPASS;
}
static void replace(Op op) {Command c{};c.op=op;if(commands) xQueueOverwrite(commands,&c);}
void listenTest() {replace(Op::ListenTest);}
void listenPackets() {replace(Op::ListenPackets);}
void stop() {replace(Op::Stop);}
bool sendPokemon(unsigned mon) {
    if(mon>2 || !commands) return false;
    Command c{};c.op=Op::SendTest;c.pokemon=mon;return xQueueSend(commands,&c,0)==pdTRUE;
}
bool sendPacket(const battle::Packet& packet) {
    if(!commands) return false;
    Command c{};c.op=Op::SendPacket;c.packet=packet;return xQueueSend(commands,&c,0)==pdTRUE;
}
Status status() {if(snapshots) xQueuePeek(snapshots,&current,0);return current;}
bool takePacket(battle::Packet& packet) {return receivedPackets && xQueueReceive(receivedPackets,&packet,0)==pdTRUE;}
}
