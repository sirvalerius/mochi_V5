#include "USB.h"
#include "USBHIDMouse.h"
#include <math.h>

#include "Settings.h"
#include "Board.h"
#include "DisplayDriver.h"
#include "MochiState.h"
#include "MochiMinigame.h"
#include "MochiView.h"
// #include "MochiServer.h"
#include "MochiBLE.h"
#include "MochiNow.h"

// --- OGGETTI GLOBALI ---
LGFX_Waveshare display;
LGFX_Sprite canvas(&display);
USBHIDMouse Mouse;

MochiState mochi;
MochiView* view;
// MochiServer* webServer;
MochiBLE* ble;
MochiNow* social;
MochiMinigame mg;

// Inizializzazione UNICA del LED
Adafruit_NeoPixel statusLed(NUM_PIXELS, PIN_RGB, NEO_GRB + NEO_KHZ800);

// --- MACCHINA A STATI PER LE ANIMAZIONI ---
enum SystemState {
  STATE_NORMAL,
  STATE_JUMPING,
  STATE_MOVING_MOUSE,
  STATE_DYING,
  STATE_DEAD_PAUSE,
  STATE_GROWING,
  STATE_GROWING_FLASH,
  STATE_MINIGAME,
  STATE_MINIGAME_RESULT
};

SystemState sysState = STATE_NORMAL;
unsigned long animStartTime = 0;
int animStep = 0;
float animLx = 40.0, animLy = 0.0;
bool lastMinigameSuccess = false;

// --- BUTTON STATE (interrupt-driven) ---
volatile bool btnFellFlag = false;
volatile bool btnRoseFlag = false;
volatile unsigned long lastISRTime = 0;

void IRAM_ATTR btnISR() {
  unsigned long t = millis();
  if (t - lastISRTime < 50) return;
  lastISRTime = t;
  if (digitalRead(PIN_BTN) == LOW) btnFellFlag = true;
  else btnRoseFlag = true;
}

// --- FUNZIONI DI UTILITA' ---
void avantiPresentazione() { Mouse.click(0x10); }
void indietroPresentazione() { Mouse.click(0x08); }

void performMouseClick() {
  // Esempio base di autoclicker non bloccante
  Mouse.click(MOUSE_LEFT);
}

// --- GESTORE ANIMAZIONI (FSM) ---
void handleAnimations(unsigned long now) {
  bool isConn = ble->isConnected();
  unsigned long elapsed = now - animStartTime;

  // --- STATO: MOVIMENTO MOUSE (Cerchio) ---
  if (sysState == STATE_MOVING_MOUSE) {
    float progress = (float)elapsed / 500.0; // Durata 0.5 secondi

    if (progress >= 1.0) {
      // Passaggio automatico al salto
      sysState = STATE_JUMPING;
      animStartTime = now;
      return;
    }

    // Calcolo cerchio basato su progress (0.0 -> 1.0 sostituisce l'angolo)
    static float lx = 0, ly = 0;
    if (elapsed < 20) { lx = 0; ly = 0; }

    float r = 25.0; 
    float angle = progress * 2.0 * M_PI; // Un giro completo
    float nx = r * cos(angle) - r;
    float ny = r * sin(angle);

    static unsigned long lastM = 0;
    if (now - lastM > 15) {
      Mouse.move((int)(nx - lx), (int)(ny - ly));
      lx = nx; ly = ny;
      lastM = now;
    }
    view->render(mochi, 0, now / 200.0f, false, isConn);

  // --- STATO: SALTO (Esultanza) ---
  } else if (sysState == STATE_JUMPING) {
    float progress = (float)elapsed / 600.0; // Durata 0.6 secondi

    if (progress >= 1.0) {
      sysState = STATE_NORMAL;
      mochi.resetTimer();
      return;
    }

    // Formula del salto basata su progress
    // Usiamo sin(progress * 2 * PI) per fare due rimbalzi (0->1->0->1->0)
    int jumpOffset = -abs(sin(progress * 2.0 * M_PI)) * 35;
    
    view->render(mochi, jumpOffset, now / 200.0f, true, isConn);

  // --- STATO: MORTE ---
  } else if (sysState == STATE_DYING) {
    float progress = (float)elapsed / 3000.0; 
    if (progress >= 1.0) {
      mochi.finalizeDeath();
      sysState = STATE_DEAD_PAUSE;
      animStartTime = now;
      return;
    }
    int offsetY = -(progress * 50); 
    int trembleX = (random(5) - 2) * (1.0 - progress); 
    view->render(mochi, offsetY + trembleX, now / 200.0f, false, isConn);

  // --- STATO: PAUSA POST-MORTE ---
  } else if (sysState == STATE_DEAD_PAUSE) {
    float progress = (float)elapsed / 2000.0;
    if (progress >= 1.0) {
      sysState = STATE_NORMAL;
      return;
    }
    view->render(mochi, 0, now / 200.0f, false, isConn);

  // --- STATO: CRESCITA/SCHIUSA ---
  } else if (sysState == STATE_GROWING) {
    float progress = (float)elapsed / 2000.0; 
    if (progress >= 1.0) {
      sysState = STATE_GROWING_FLASH;
      animStartTime = now;
      return;
    }
    view->drawGrowthFrame(progress, mochi.currentAge, mochi.targetGrowthStage);

  // --- STATO: FLASH FINALE ---
  } else if (sysState == STATE_GROWING_FLASH) {
    float progress = (float)elapsed / 500.0;
    if (progress >= 1.0) {
      mochi.finalizeGrowth();
      sysState = STATE_NORMAL;
      return;
    }
    canvas.fillScreen(K_WHITE);
    canvas.pushSprite(0, 0);
  }
}

//----------------- SETUP ---------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  USB.begin();
  Mouse.begin();

  // Rileva la variante hardware (ST7789 vs Touch JD9853) PRIMA di toccare
  // il display: popola g_board con i pin corretti.
  boardDetect();

  // Inizializzazione hardware (LGFX) con i pin della board rilevata
  display.configure(g_board);
  display.init();
  display.setRotation(g_board.rotation);
  
  canvas.createSprite(display.width(), display.height());

  // Inizializzazione Stato e View
  mochi.begin();
  view = new MochiView(&canvas);

  pinMode(g_board.bl, OUTPUT);
  analogWrite(g_board.bl, mochi.screenBrightness);

  pinMode(PIN_BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN), btnISR, CHANGE);

  // Inizializzazione LED Globale (solo se la board lo ha: sulla variante
  // Touch il GPIO38 e' il clock LCD, quindi il NeoPixel resta spento).
  if (g_board.hasStatusLed) {
    statusLed.setPin(g_board.rgbPin);
    statusLed.begin();
    statusLed.setBrightness(50);
    statusLed.setPixelColor(0, statusLed.Color(0, 0, 0));
    statusLed.show();
  }

  // Inizializzazione Rete (Passiamo il puntatore al LED!)
  // webServer = new MochiServer(&mochi, &statusLed);
  // webServer->begin();
  
  ble = new MochiBLE(&mochi, g_board.hasStatusLed ? &statusLed : nullptr);
  ble->begin();

  // Discovery e visite tra Mochi ora via ESP-NOW (BLE resta solo per la app)
  social = new MochiNow(&mochi);
  social->begin();
  ble->attachSocial(social);

  mochi.resetTimer();
}

//----------------- LOOP  ---------------------------------------------------------------

void loop() {
  unsigned long now = millis();
  bool isConnected = true;

  // --- BUTTON (ISR flags) ---
  bool justPressed = false, justReleased = false;
  noInterrupts();
  if (btnFellFlag) { justPressed  = true; btnFellFlag = false; }
  if (btnRoseFlag) { justReleased = true; btnRoseFlag = false; }
  interrupts();
  bool btnHeld = (digitalRead(PIN_BTN) == LOW);

  // --- MINIGAME STATE ---
  if (sysState == STATE_MINIGAME) {
    mg.tick(now, justPressed, btnHeld, justReleased);
    view->drawMinigame(mg, now);
    if (mg.complete) {
      if (mg.success) {
        mochi.gainFromMinigame(mochi.pendingAction, mg.score);
        ble->pushState();
      }
      mochi.minigamePlayedThisSlot = true;
      lastMinigameSuccess = mg.success;
      sysState     = STATE_MINIGAME_RESULT;
      animStartTime = now;
    }
    delay(15);
    return;
  }

  if (sysState == STATE_MINIGAME_RESULT) {
    if (now - animStartTime >= 1500) { sysState = STATE_NORMAL; }
    else { view->drawMinigameResult(lastMinigameSuccess); }
    delay(15);
    return;
  }

  // --- ANIMAZIONI STANDARD (Uscita anticipata se in corso) ---
  if (sysState != STATE_NORMAL) {
    handleAnimations(now);
    delay(15);
    return;
  }

  // 3. INTERCETTAZIONE TRIGGER STATI SPECIALI
  if (mochi.isDying) {
    sysState = STATE_DYING;
    animStep = 0;
    animStartTime = now;
    return;
  }

  if (mochi.needsGrowthAnimation) {
    sysState = STATE_GROWING;
    animStep = 0;
    animStartTime = now;
    return;
  } 
  
  if (mochi.lastCommand != "") {
     mochi.isHeartVisible = true;
     mochi.heartShowTime = millis();
     if (mochi.lastCommand == "prev") indietroPresentazione();
     else if (mochi.lastCommand == "next") avantiPresentazione();
     mochi.lastCommand = ""; 
  } else {
     mochi.triggerHeart(); 
  }

  // --- BUTTON: launch minigame for queued action ---
  mochi.updateBubbles();
  if (justPressed && mochi.pendingAction != ACTION_NONE && !mochi.minigamePlayedThisSlot && !mochi.isAway) {
    MinigameType mgType = MG_NONE;
    switch (mochi.pendingAction) {
      case ACTION_FEED:      mgType = MG_CHEW;     break;
      case ACTION_PET:       mgType = MG_SURPRISE; break;
      case ACTION_TRAIN_STR: mgType = MG_MASH;     break;
      case ACTION_TRAIN_SPD: mgType = MG_REACT;    break;
      case ACTION_TRAIN_INT: mgType = MG_COUNT;    break;
      case ACTION_TRAIN_CHR: mgType = MG_HOLD;     break;
      default: break;
    }
    if (mgType != MG_NONE) {
      mg.begin(mgType, now);
      sysState = STATE_MINIGAME;
      delay(15);
      return;
    }
  }

  // Calcolo Animazione Base (Respiro/Rimbalzo)
  float bounce = -abs(sin(fmod(now / 1000.0 * 2.5, M_PI))) * 12;
  bool wink = (fmod(now, 5000) < 200);
  float animAngle = now / 200.0;

  // --- SOCIAL (ESP-NOW): annuncio presenza, discovery vicini, logica visite ---
  social->tick(now);
  mochi.isFriendNearby = (social->nearbyCount() > 0);

  // --- VISITE: scadenze (ritorno a casa / fine ospitalità) ---
  if (mochi.isAway && now >= mochi.awayUntil) mochi.returnHome();
  if (mochi.isHostingGuest && now >= mochi.guestUntil) mochi.guestLeaves();

  // Disegno a schermo
  // --- CONTROLLO AGGIORNAMENTO COLORI ---
  if (mochi.colorsUpdated) {
    view->setBackgroundColors(mochi.bgTopColor, mochi.bgBottomColor);
    mochi.colorsUpdated = false; // Resetta il flag
  }
  view->render(mochi, (int)bounce, animAngle, wink, isConnected);

  // Autoclicker
  if(mochi.isAutoClickActive) {
    performMouseClick();
  }

  // Routine Automatica (Jiggler/Salti) — in pausa mentre il Mochi è in visita
  if (!mochi.isAway && mochi.timeForAction()) {

    mochi.resetTimer();
    mochi.applyTick();

    if(!mochi.isAutoClickActive) {
      mochi.resetTimer();
      sysState = STATE_MOVING_MOUSE;
      animStep = 0;
      animStartTime = now;
    } else {
      
    }
  }
}