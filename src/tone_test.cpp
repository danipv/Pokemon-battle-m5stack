// Quick half-duplex speaker/microphone transport test. NFC/NVS is untouched.
#include <M5Unified.h>
#include "board.h"
#include "tone_protocol.h"

static constexpr uint16_t BG=0x08A4,ACCENT=0x4DFF;
static const char* NAMES[]={"Bulbasaur","Charmander","Squirtle"};
enum class Mode { Idle,Listening,Sending };
static Mode mode=Mode::Idle;
static tones::Decoder decoder;
static int selected=0,lastReceived=-1;
static uint32_t received=0,lastUi=0,nextStep=0;
static int sendStep=0;
static int16_t samples[tones::BLOCK];
static bool pending=false;
static String status="Pulsa Escuchar o Emitir";
static auto& lcd=M5.Display;

static void text(const String& s,int x,int y,uint16_t color=TFT_WHITE) {
    lcd.setTextColor(color,BG); lcd.drawString(s,x,y);
}
static void draw() {
    lcd.fillScreen(BG);
    text("PRUEBA TONOS v0.1",12,8,ACCENT);
    for(int i=0;i<3;++i) {
        const int x=6+105*i;
        lcd.fillRoundRect(x,34,99,36,7,i==selected ? 0x1C73 : 0x2188);
        lcd.setTextColor(TFT_WHITE,i==selected ? 0x1C73 : 0x2188);
        lcd.drawString(NAMES[i],x+5,44);
    }
    lcd.fillRoundRect(8,82,148,44,8,0x226A);
    lcd.fillRoundRect(164,82,148,44,8,0x2286);
    text("Emitir tonos",25,96); text("Escuchar",200,96);
    text(status,8,137,ACCENT);
    if(mode==Mode::Listening) {
        char live[64];
        snprintf(live,sizeof(live),"Nivel %.0f  Tono %d Hz",decoder.rms,decoder.tone<0 ? 0 : tones::FREQUENCIES[decoder.tone]);
        text(live,8,159);
    }
    if(lastReceived>=0) text(String("RX: ")+NAMES[lastReceived]+"  #"+received,8,180,TFT_GREEN);
    lcd.fillRoundRect(8,204,304,30,7,0x2188);
    text("Parar",136,211);
}

static void stopAudio() {
    // End the recorder before reusing its buffer or giving I2S to the speaker.
    I2CGuard lock;
    if(M5.Mic.isRunning()) M5.Mic.end();
    pending=false;
    if(M5.Speaker.isRunning()) {M5.Speaker.stop();M5.Speaker.end();}
    mode=Mode::Idle;
}

static void listen() {
    stopAudio();
    {
        I2CGuard lock;
        if(!M5.Mic.begin()) {status="Error microfono. Reinicia";draw();return;}
    }
    decoder.reset();
    mode=Mode::Listening;status="ESCUCHANDO...";draw();
    Serial.println("[tones] listening 16000 Hz");
}

static void transmit() {
    stopAudio();
    {
        I2CGuard lock;
        if(!M5.Speaker.begin()) {status="Error altavoz. Reinicia";draw();return;}
        M5.Speaker.setVolume(140);
    }
    mode=Mode::Sending;sendStep=0;nextStep=millis();
    status=String("TX: ")+NAMES[selected];draw();
    Serial.printf("[tones] sending %s\n",NAMES[selected]);
}

static void serialCommands() {
    static char line[32];static unsigned used=0;
    while(Serial.available()) {
        const char c=Serial.read(); if(c=='\r') continue;
        if(c!='\n') {if(used<sizeof(line)-1) line[used++]=c;continue;}
        line[used]=0;used=0;
        if(!strcmp(line,"listen")) listen();
        else if(!strcmp(line,"stop")) {stopAudio();status="Parado";draw();}
        else if(!strncmp(line,"send ",5) && line[5]>='0' && line[5]<='2') {selected=line[5]-'0';transmit();}
    }
}

void setup() {
    auto cfg=M5.config();cfg.serial_baudrate=115200;
    M5.begin(cfg);Serial.setTxTimeoutMs(0);
    lcd.setRotation(1);lcd.setBrightness(170);
    lcd.setFont(&fonts::efontJA_16);lcd.setTextDatum(top_left);lcd.setTextWrap(false);
    boardInit(true);servoInit();delay(600);setBaseLeds(0,0,0);
    stopAudio();draw();
    Serial.println("[tones] v0.1 ready; commands: listen | send 0 | send 1 | send 2 | stop");
}

void loop() {
    {I2CGuard lock;M5.update();}
    serialCommands();
    const auto touch=M5.Touch.getDetail();
    if(touch.wasClicked()) {
        if(touch.y>=34 && touch.y<70 && mode!=Mode::Sending) {selected=constrain(touch.x/105,0,2);draw();}
        else if(touch.y>=82 && touch.y<126) {if(touch.x<160) transmit();else listen();}
        else if(touch.y>=204) {stopAudio();status="Parado";draw();}
    }
    const uint32_t now=millis();
    if(mode==Mode::Sending && (int32_t)(now-nextStep)>=0) {
        if(sendStep>=6) {
            M5.Speaker.stop();mode=Mode::Idle;status="Enviado. Repite si hace falta";draw();
            Serial.println("[tones] sent");
        } else if(!(sendStep&1)) {
            const int hz=sendStep==0 ? 2400 : sendStep==2 ? tones::FREQUENCIES[selected] : 2800;
            M5.Speaker.tone(hz,tones::NOTE_MS);
            nextStep=now+tones::NOTE_MS;++sendStep;
        } else {
            M5.Speaker.stop();nextStep=now+tones::GAP_MS;++sendStep;
        }
    }
    if(mode==Mode::Listening) {
        if(pending && !M5.Mic.isRecording()) {
            const int decoded=decoder.consume(samples,now);
            pending=false;
            if(decoded>=0) {
                lastReceived=decoded;++received;
                Serial.printf("[tones] RX %s count=%lu rms=%.0f confidence=%.2f\n",NAMES[decoded],(unsigned long)received,decoder.rms,decoder.confidence);
                draw();
            }
        }
        if(!pending) {
            pending=M5.Mic.record(samples,tones::BLOCK,tones::RATE);
            if(!pending) {stopAudio();status="Error capturando microfono";draw();}
        }
        // Refresh a small strip while asynchronous microphone capture continues.
        if(now-lastUi>=1000) {
            lastUi=now;
            Serial.printf("[tones] rms=%.0f tone=%d confidence=%.2f\n",decoder.rms,decoder.tone<0 ? 0 : tones::FREQUENCIES[decoder.tone],decoder.confidence);
            // This is a small text strip; capture runs asynchronously in the mic task.
            lcd.fillRect(0,156,320,22,BG);
            char live[64];snprintf(live,sizeof(live),"Nivel %.0f  Tono %d Hz",decoder.rms,decoder.tone<0 ? 0 : tones::FREQUENCIES[decoder.tone]);
            text(live,8,159);
        }
    }
    delay(1);
}
