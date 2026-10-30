// NFC battles over an acoustic packet modem. NFC contents are untouched.
// Audio has one dedicated owner; the UI uses nonblocking queues and I2C try-locks.
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <Preferences.h>
#include "board.h"
#include "stackchan_limits.h"
#include "animations.h"
#include "battle.h"
#include "audio_modem.h"
#include "acoustic_packet.h"

static battle::Engine game(1800,60000,120000);
static uint32_t nextTx=0,lastAudioStatus=0,finishedAt=0,finalTransmits=0;
static bool sendBye=false;
static uint16_t byeSession=0;
static uint8_t byeRound=0,byeRepeats=0;
static sound::Status audioState;

static constexpr uint16_t BG = 0x08A4; // RGB(13,22,35), same as GIF frame backgrounds
static constexpr uint16_t MUTED = 0x9D16;
static const char* NAMES[] = {"Bulbasaur", "Charmander", "Squirtle"};
static const char* TYPES[] = {"PLANTA / VENENO", "FUEGO", "AGUA"};
static const char* KEYS[] = {"bulbasaur", "charmander", "squirtle"};
static const uint16_t COLORS[] = {0x6E6C, 0xFC29, 0x4DFF};
static const uint8_t LEDS[3][3] = {{0,LED_MAX_LEVEL,0},{LED_MAX_LEVEL,20,0},{0,20,LED_MAX_LEVEL}};
static m5::unit::UnitUnified units;
static m5::unit::UnitNFC unit{0x50};
static m5::nfc::NFCLayerA nfc{unit};
static Preferences prefs;
static M5Canvas frame(&M5.Display);
static String cards[3], draft[3], lastUid;
static uint32_t lastSeen, lastPoll, nextFrame, messageUntil;
static int selected = 0, step = 0;
static uint16_t frameIndex;
static uint16_t enemyFrameIndex;
static uint32_t nextEnemyFrame;
static bool configuring = false, initialSetup = true, haveTag = false, storageReady = false;
static bool ready = false, frameReady = false;
static String message;

static auto& lcd = M5.Display;
static void drawPage();
static void notify(const String&,bool);

static void safeLeds(uint8_t r,uint8_t g,uint8_t b) {
    if(i2cTryLock()) {setBaseLeds(r,g,b);i2cUnlock();}
}

static void stopBattle() {
    if (game.paired()) {
        sendBye=true; byeSession=game.session; byeRound=game.round; byeRepeats=3;
    }
    game.stop(); haveTag=false;
    safeLeds(0,0,0);
}

static void choosePokemon(int mon) {
    stopBattle();
    finishedAt=0;finalTransmits=0;
    selected=mon; haveTag=true;
    uint16_t token=0;
    while (!token) token=esp_random()&65535;
    game.choose(token,mon,millis());
    nextTx=millis()+600+esp_random()%2000;
    Serial.printf("[battle] selected=%s token=%04X\n",NAMES[mon],token);
}

static bool validCards(const String* list) {
    for (int i=0; i<3; ++i) {
        if (list[i].isEmpty()) return false;
        for (int j=0; j<i; ++j) if (list[i] == list[j]) return false;
    }
    return true;
}

static void text(const String& s, int x, int y, uint16_t color=TFT_WHITE) {
    lcd.setTextColor(color, BG);
    lcd.drawString(s, x, y);
}

static void footer() {
    lcd.fillRect(0, 198, 320, 42, BG);
    if (configuring) {
        lcd.fillRoundRect(8,204,304,30,8,0x2188);
        text(initialSetup ? "Reiniciar asignacion" : "Cancelar / volver",20,210);
    } else {
        lcd.fillRoundRect(8,204,144,30,8,0x2188);
        lcd.fillRoundRect(160,204,152,30,8,0x2188);
        text("Tarjetas",32,210);
        text(game.state==battle::State::Idle ? "Elegir NFC" : "Salir / nuevo",172,210);
    }
}

static void life(int x,int hp,uint16_t color) {
    lcd.fillRoundRect(x,34,144,9,3,0x2188);
    if (hp>0) lcd.fillRoundRect(x,34,144*hp/100,9,3,color);
}

static void drawBattle() {
    const bool paired=game.paired();
    text(NAMES[selected],8,8,COLORS[selected]);
    text(paired ? NAMES[game.opponent] : "Rival: ...",168,8,paired ? COLORS[game.opponent] : MUTED);
    life(8,paired ? game.myHp() : 100,COLORS[selected]);
    life(168,paired ? game.theirHp() : 100,paired ? COLORS[game.opponent] : MUTED);
    if (!paired) {
        lcd.drawCircle(240,108,25,MUTED);
        text("?",236,100,MUTED);
    }
    String line;
    switch (game.state) {
        case battle::State::Idle: line="Elige Pokemon con una tarjeta"; break;
        case battle::State::Searching: line="Buscando rival por tonos"; break;
        case battle::State::Fighting:
            if (!game.round) line="Combate: preparados";
            else line=String("Turno ")+game.round+"  "+(game.lastAttacker==game.mySide() ? "Atacas" : "Te atacan")+" -"+game.lastDamage+" PS";
            break;
        case battle::State::Finished: line=game.won() ? "VICTORIA!" : "DERROTA. Otra oportunidad!"; break;
        case battle::State::LinkLost: line="Sin enlace. Pulsa Salir / nuevo"; break;
    }
    text(line,8,179,game.state==battle::State::Finished ? (game.won() ? TFT_GREEN : TFT_ORANGE) : MUTED);
    frameIndex=enemyFrameIndex=0; nextFrame=nextEnemyFrame=0;
}

static void drawPage() {
    lcd.fillScreen(BG);
    lcd.fillRect(0, 0, 320, 3, COLORS[selected]);
    if (configuring) {
        text("CONFIGURAR TARJETAS", 12, 10, COLORS[step]);
        text(String(step+1) + " / 3   " + NAMES[step], 12, 37);
        text("Acerca una tarjeta a la base", 12, 65, MUTED);
        text("Usa una distinta para cada uno", 12, 85, MUTED);
        for (int i=0; i<3; ++i) {
            text(String(draft[i].isEmpty() ? "[ ] " : "[OK] ") + NAMES[i], 16, 112+i*23,
                 i==step ? COLORS[i] : MUTED);
        }
    } else {
        drawBattle();
    }
    footer();
}

static void notify(const String& msg, bool error=false) {
    message=msg;
    messageUntil=millis()+1800;
    lcd.fillRect(0,177,320,21,BG);
    text(msg,8,177,error ? TFT_ORANGE : TFT_WHITE);
    Serial.printf("[pokemon] %s\n",msg.c_str());
}

static void beginConfig() {
    stopBattle();
    configuring=true;
    step=0;
    messageUntil=0;
    for (auto& uid : draft) uid="";
    // Keep lastUid: a card left on the base must be removed before assigning it.
    drawPage();
}

static bool saveDraft() {
    // One NVS blob/string makes the three assignments an atomic update.
    const String data=draft[0]+"|"+draft[1]+"|"+draft[2];
    return storageReady && prefs.putString("cards",data)==data.length();
}

static void onCard(const String& uid) {
    Serial.printf("[pokemon] UID %s\n",uid.c_str());
    if (configuring) {
        for (int i=0; i<step; ++i) {
            if (draft[i]==uid) { notify("Tarjeta repetida: usa otra",true); return; }
        }
        draft[step]=uid;
        ++step;
        if (step==3) {
            if (!saveDraft()) {
                --step;
                draft[step]="";
                drawPage();
                notify("Error al guardar. Reintenta",true);
                return;
            }
            for (int i=0; i<3; ++i) cards[i]=draft[i];
            initialSetup=false;
            configuring=false;
            selected=2;
            haveTag=false;
            safeLeds(LEDS[2][0],LEDS[2][1],LEDS[2][2]);
            drawPage();
            notify("Guardado. Prueba tus tarjetas");
        } else drawPage();
        return;
    }
    int match=-1;
    for (int i=0; i<3; ++i) if (cards[i]==uid) match=i;
    if (match<0) { notify("Tarjeta sin asignar",true); return; }
    if (game.state==battle::State::Fighting) return;
    if (game.state==battle::State::Searching && selected==match) return;
    choosePokemon(match);
    selected=match;
    haveTag=true;
    messageUntil=0;
    safeLeds(LEDS[match][0],LEDS[match][1],LEDS[match][2]);
    drawPage();
}

void setup() {
    auto cfg=M5.config();
    cfg.serial_baudrate=115200;
    M5.begin(cfg);
    Serial.setTxTimeoutMs(0);
    lcd.setRotation(1);
    lcd.setBrightness(170);
    lcd.setFont(&fonts::efontJA_16);
    lcd.setTextDatum(top_left);
    lcd.setTextWrap(false);
    lcd.fillScreen(BG);
    text("COMBATE POR TONOS: iniciando...",12,20);
    boardInit(true);
    servoInit(); // center pan and level tilt using the calibrated home positions
    delay(600);
    safeLeds(0,0,0);
    storageReady=prefs.begin("pokemon-nfc",false);
    if (!storageReady) { text("Error memoria: reinicia el robot",12,65,TFT_ORANGE); return; }
    const String saved=prefs.isKey("cards") ? prefs.getString("cards","") : "";
    const int sep=saved.indexOf('|'), end=saved.indexOf('|',sep+1);
    if (sep>0 && end>sep) {
        cards[0]=saved.substring(0,sep);
        cards[1]=saved.substring(sep+1,end);
        cards[2]=saved.substring(end+1);
    }
    initialSetup=!validCards(cards);
    frame.setPsram(true);
    frame.setColorDepth(16);
    frameReady=frame.createSprite(160,128)!=nullptr;
    if (!frameReady) { text("Error animacion: sin memoria",12,65,TFT_ORANGE); return; }
    if (!units.add(unit,M5.In_I2C) || !units.begin()) {
        text("NFC no responde. Revisa la base",12,65,TFT_ORANGE);
        Serial.println("[pokemon] NFC init failed");
        return;
    }
    ready=true;
    if(!sound::begin()) {ready=false;text("Error creando audio",12,65,TFT_ORANGE);return;}
    sound::listenPackets();
    Serial.printf("[pokemon] ready, configured=%d heap=%u PSRAM=%u\n",!initialSetup,ESP.getFreeHeap(),ESP.getFreePsram());
    if (initialSetup) beginConfig(); else drawPage();
}

void loop() {
    const bool touchReady=i2cTryLock();
    if(touchReady) {M5.update();i2cUnlock();}
    if (!ready) { delay(10); return; }
    const uint32_t now=millis();
    const auto touch=M5.Touch.getDetail();
    if (touchReady && touch.wasClicked() && touch.y>=204) {
        if (!configuring && touch.x>=160) { stopBattle(); messageUntil=0; drawPage(); }
        else if (!configuring) beginConfig();
        else if (initialSetup) beginConfig();
        else { configuring=false; messageUntil=0; drawPage(); }
    }
    if (messageUntil && (int32_t)(now-messageUntil)>=0) {
        messageUntil=0;
        drawPage();
    }
    if (!configuring && (int32_t)(now-nextFrame)>=0) {
        const auto& animation=ANIMATIONS[selected];
        const auto& f=animation.frames[frameIndex];
        frame.drawJpg(f.data,f.size,0,0);
        frame.pushSprite(0,48);
        nextFrame=now+f.ms;
        frameIndex=(frameIndex+1)%animation.count;
    }
    if (!configuring && game.paired() && (int32_t)(now-nextEnemyFrame)>=0) {
        const auto& animation=ANIMATIONS[game.opponent];
        const auto& f=animation.frames[enemyFrameIndex];
        frame.drawJpg(f.data,f.size,0,0);
        frame.pushSprite(160,48);
        nextEnemyFrame=now+f.ms;
        enemyFrameIndex=(enemyFrameIndex+1)%animation.count;
    }
    audioState=sound::status();
    battle::Packet incoming{};
    while(sound::takePacket(incoming)) {
        Serial.printf("[sound] RX kind=%u id=%04X round=%u\n",unsigned(incoming.kind),incoming.id,incoming.round);
        if(!configuring) {
            if(game.state==battle::State::Searching && incoming.kind==battle::Kind::Hello && incoming.id==game.token)
                choosePokemon(selected);
            else game.receive(incoming,now);
            nextTx=now+650+esp_random()%400;
        }
    }
    game.tick(now);
    if (game.dirty && !configuring) {
        game.dirty=false; messageUntil=0; drawPage();
        Serial.printf("[battle] state=%d round=%u hp=%d/%d peer=%04X\n",int(game.state),game.round,game.myHp(),game.theirHp(),game.peer);
        if (game.state==battle::State::Finished) {
            if(!finishedAt) finishedAt=now;
            safeLeds(game.won() ? 0 : LED_MAX_LEVEL,game.won() ? LED_MAX_LEVEL : 0,0);
        }
    }
    const bool audioHealthy=!audioState.heartbeat || now-audioState.heartbeat<1500;
    if((int32_t)(now-nextTx)>=0 && audioHealthy && audioState.mode==sound::Mode::Listening
       && !audioState.inFrame && now-audioState.lastSignal>300) {
        battle::Packet outgoing{};bool transmit=false;
        if(sendBye && byeRepeats) {
            outgoing={battle::Kind::Bye,byeSession,0,byeRound};transmit=true;
            if(!--byeRepeats) sendBye=false;
        } else if(!configuring && (game.state!=battle::State::Finished
                  || (now-finishedAt<60000 && (finalTransmits<3 || game.peerRound<game.round))))
            transmit=game.transmit(outgoing);
        if(transmit && sound::sendPacket(outgoing)) {
            if(game.state==battle::State::Finished && outgoing.kind==battle::Kind::Sync) ++finalTransmits;
            nextTx=now+acoustic::FRAME_MS+1800+esp_random()%2000;
            Serial.printf("[sound] TX kind=%u id=%04X round=%u\n",unsigned(outgoing.kind),outgoing.id,outgoing.round);
        }
    }
    if(now-lastAudioStatus>=2000) {
        lastAudioStatus=now;
        if(!configuring && (audioState.mode==sound::Mode::Error || !audioHealthy)) {
            lcd.fillRect(0,177,320,21,BG);text("Audio detenido. Reinicia",8,179,TFT_ORANGE);
        } else if(!configuring && game.state==battle::State::Searching && !messageUntil) {
            lcd.fillRect(0,177,320,21,BG);char line[48];
            snprintf(line,sizeof(line),"Tonos TX:%lu RX:%lu CRC:%lu",(unsigned long)audioState.sent,(unsigned long)audioState.received,(unsigned long)audioState.rejected);
            text(line,8,179,MUTED);
        }
    }
    if(i2cTryLock()) {units.update();i2cUnlock();}
    if (now-lastPoll>=200) {
        lastPoll=now;
        m5::nfc::a::PICC picc{};
        if (!i2cTryLock()) {delay(1);return;}
        const bool detected=nfc.detect(picc);
        if (detected) {
            const String uid=picc.uidAsString().c_str();
            const bool repeat=uid==lastUid && now-lastSeen<1200;
            lastSeen=now;
            lastUid=uid;
            nfc.deactivate();
            i2cUnlock();
            if (!repeat) onCard(uid);
        } else i2cUnlock();
    }
    delay(2);
}
