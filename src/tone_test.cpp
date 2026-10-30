// Quick half-duplex speaker/microphone transport test. NFC/NVS is untouched.
#include <M5Unified.h>
#include "board.h"
#include "audio_modem.h"

static constexpr uint16_t BG=0x08A4,ACCENT=0x4DFF;
static const char* NAMES[]={"Bulbasaur","Charmander","Squirtle"};
using Mode=sound::Mode;
static Mode mode=Mode::Idle;
static sound::Status audioState;
static int selected=0,lastReceived=-1;
static uint32_t received=0,lastUi=0;
static String status="Pulsa Escuchar o Emitir";
static auto& lcd=M5.Display;

static void text(const String& s,int x,int y,uint16_t color=TFT_WHITE) {
    lcd.setTextColor(color,BG); lcd.drawString(s,x,y);
}
static void draw() {
    lcd.fillScreen(BG);
    text("PRUEBA TONOS v0.2",12,8,ACCENT);
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
        snprintf(live,sizeof(live),"Nivel %.0f  Tono %d Hz",audioState.rms,audioState.toneHz);
        text(live,8,159);
    }
    if(lastReceived>=0) text(String("RX: ")+NAMES[lastReceived]+"  #"+received,8,180,TFT_GREEN);
    lcd.fillRoundRect(8,204,304,30,7,0x2188);
    text("Parar",136,211);
}

static void stopAudio() {sound::stop();mode=Mode::Idle;}
static void listen() {sound::listenTest();status="Iniciando escucha...";draw();}
static void transmit() {
    status=sound::sendPokemon(selected) ? String("TX: ")+NAMES[selected] : "Audio ocupado. Reintenta";
    draw();
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
    if(!sound::begin()) status="Error creando audio";
    draw();
    Serial.println("[tones] v0.2 ready; commands: listen | send 0 | send 1 | send 2 | stop");
}

void loop() {
    const bool touchReady=i2cTryLock();
    if(touchReady) {M5.update();i2cUnlock();}
    serialCommands();
    const auto touch=M5.Touch.getDetail();
    if(touchReady && touch.wasClicked()) {
        if(touch.y>=34 && touch.y<70 && mode!=Mode::Sending) {selected=constrain(touch.x/105,0,2);draw();}
        else if(touch.y>=82 && touch.y<126) {if(touch.x<160) transmit();else listen();}
        else if(touch.y>=204) {stopAudio();status="Parando audio...";draw();}
    }
    const uint32_t now=millis();
    audioState=sound::status();
    if(audioState.mode!=mode) {
        const Mode previous=mode;mode=audioState.mode;
        if(mode==Mode::Listening) status="ESCUCHANDO...";
        else if(mode==Mode::Sending) status=String("TX: ")+NAMES[selected];
        else if(mode==Mode::Error) status="Error audio. Pulsa Escuchar";
        else status=previous==Mode::Sending ? "Enviado" : "Parado";
        draw();
    }
    if(audioState.received!=received && audioState.pokemon>=0) {
        received=audioState.received;lastReceived=audioState.pokemon;draw();
    }
    if(now-lastUi>=250) {
        lastUi=now;
        if(audioState.heartbeat && now-audioState.heartbeat>1500 && mode!=Mode::Idle) {
            lcd.fillRect(0,134,320,22,BG);text("Audio sin respuesta. Reinicia",8,137,TFT_ORANGE);
        }
        if(mode==Mode::Listening) {
            lcd.fillRect(0,156,320,22,BG);
            char live[64];snprintf(live,sizeof(live),"Nivel %.0f  Tono %d Hz",audioState.rms,audioState.toneHz);
            text(live,8,159);
        }
    }
    delay(1);
}
