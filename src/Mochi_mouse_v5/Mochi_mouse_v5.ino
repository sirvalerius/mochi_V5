#include "USB.h"
#include "USBHIDMouse.h"
#include <math.h>

#include "Settings.h"
#include "DisplayDriver.h"
#include "MochiState.h"
#include "MochiView.h"
// #include "MochiServer.h" 
#include "MochiBLE.h"

// --- OGGETTI GLOBALI ---
LGFX_Waveshare display;
LGFX_Sprite canvas(&display);
USBHIDMouse Mouse;

MochiState mochi;
MochiView* view;
// MochiServer* webServer; 
MochiBLE* ble;

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
  STATE_GROWING_FLASH   
};

SystemState sysState = STATE_NORMAL;
unsigned long animStartTime = 0;
int animStep = 0;
float animLx = 40.0, animLy = 0.0;

// --- FUNZIONI DI UTILITA' ---
void avantiPresentazione() { Mouse.click(0x10); }
void indietroPresentazione() { Mouse.click(0x08); }

void performMouseClick() {
  // Esempio base di autoclicker non bloccante
  Mouse.click(MOUSE_LEFT);
}

// --- GESTORE ANIMAZIONI (FSM) ---
void handleAnimations(unsigned long now) {
  bool isConn = ble->isConnected(); // Serve per il render

  // --- ANIMAZIONE SALTO (I vecchi 5 minuti) ---
  if (sysState == STATE_JUMPING) {
    if (elapsed > 600) { // Il salto dura un po' di più per essere visibile
      sysState = STATE_NORMAL;
      return;
    }
    int jumpOffset = -abs(sin((elapsed / 600.0) * 2.0 * M_PI)) * 35; // Effetto rimbalzo: Mochi salta due volte (usando sin^2 o valore assoluto) Con 2.0 * M_PI fa due balzi nel tempo dell'animazione

    view->render(mochi, jumpOffset, now / 200.0f, true, isConn);
  } else if (sysState == STATE_MOVING_MOUSE) {
    if (elapsed > 1200) { // Durata cerchio
      // FINE MOUSE -> INIZIO SALTO
      sysState = STATE_JUMPING;
      animStartTime = now; // Reset del cronometro per il salto
      return;
    }

    static float lx = 0, ly = 0;
    if (elapsed < 20) { lx = 0; ly = 0; }

    float r = 25.0; 
    float angle = (elapsed / 1200.0) * 2.0 * M_PI;
    float nx = r * cos(angle) - r;
    float ny = r * sin(angle);

    static unsigned long lastM = 0;
    if (now - lastM > 15) {
      Mouse.move((int)(nx - lx), (int)(ny - ly));
      lx = nx; ly = ny;
      lastM = now;
    }
    view->render(mochi, 0, now / 200.0f, false, isConn);
  } else if (sysState == STATE_DYING) {
    float progress = (float)(now - animStartTime) / 3000.0; 
    if (progress >= 1.0) {
      mochi.finalizeDeath();
      sysState = STATE_DEAD_PAUSE;
      animStartTime = now;
      return;
    }
    int offsetY = -(progress * 50); 
    int trembleX = (random(5) - 2) * (1.0 - progress); 
    view->render(mochi, offsetY + trembleX, now / 200.0f, false, isConn);

  // --- PAUSA DA MORTO (2 Secondi) ---
  } else if (sysState == STATE_DEAD_PAUSE) {
    float progress = (float)(now - animStartTime) / 2000.0;
    if (progress >= 1.0) {
      sysState = STATE_NORMAL;
      return;
    }
    view->render(mochi, 0, now / 200.0f, false, isConn);

  // --- ANIMAZIONE CRESCITA/SCHIUSA (2 Secondi) ---
  } else if (sysState == STATE_GROWING) {
    float progress = (float)(now - animStartTime) / 2000.0; 
    if (progress >= 1.0) {
      sysState = STATE_GROWING_FLASH;
      animStartTime = now;
      return;
    }
    view->drawGrowthFrame(progress, mochi.currentAge, mochi.targetGrowthStage);

  // --- FLASH LUMINOSO FINALE (0.5 Secondi) ---
  } else if (sysState == STATE_GROWING_FLASH) {
    float flashProgress = (float)(now - animStartTime) / 500.0;
    if (flashProgress >= 1.0) {
      mochi.finalizeGrowth();
      sysState = STATE_NORMAL;
      return;
    }
    canvas.fillScreen(K_WHITE);
    canvas.pushSprite(0, 0);
  }
}

void setup() {
  Serial.begin(115200);
  USB.begin();
  Mouse.begin();

  // Inizializzazione hardware (LGFX)
  display.init();
  display.setRotation(1);
  pinMode(PIN_BL, OUTPUT); 
  digitalWrite(PIN_BL, HIGH); 
  canvas.createSprite(display.width(), display.height());

  // Inizializzazione Stato e View
  mochi.begin();
  view = new MochiView(&canvas);

  // Inizializzazione LED Globale
  statusLed.begin();
  statusLed.setBrightness(50); 
  statusLed.setPixelColor(0, statusLed.Color(0, 0, 0));
  statusLed.show();

  // Inizializzazione Rete (Passiamo il puntatore al LED!)
  // webServer = new MochiServer(&mochi, &statusLed);
  // webServer->begin();
  
  ble = new MochiBLE(&mochi, &statusLed);
  ble->begin();

  mochi.resetTimer();
}

void loop() {
  unsigned long now = millis();
  bool isConnected = true;

  // 2. GESTIONE ANIMAZIONI (Uscita anticipata se in corso)
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

  // 4. LOGICA STANDARD E AGGIORNAMENTO STATISTICHE
  mochi.updateDecay();
  mochi.checkLifecycle();
  
  if (mochi.lastCommand != "") {
     mochi.isHeartVisible = true;
     mochi.heartShowTime = millis();
     if (mochi.lastCommand == "prev") indietroPresentazione();
     else if (mochi.lastCommand == "next") avantiPresentazione();
     mochi.lastCommand = ""; 
  } else {
     mochi.triggerHeart(); 
  }

  // Calcolo Animazione Base (Respiro/Rimbalzo)
  float bounce = -abs(sin(fmod(now / 1000.0 * 2.5, M_PI))) * 12;
  bool wink = (fmod(now, 5000) < 200);
  float animAngle = now / 200.0;

  // Disegno a schermo
  view->render(mochi, (int)bounce, animAngle, wink, isConnected);

  // Autoclicker
  if(mochi.isAutoClickActive) {
    performMouseClick();
  }

  // Routine Automatica (Jiggler/Salti)
  if (mochi.timeForAction()) {
    mochi.recharge();
    if(!mochi.isAutoClickActive) {
      mochi.resetTimer();
      sysState = STATE_MOVING_MOUSE;
      animStep = 0;
      animStartTime = now;
    } else {
      mochi.resetTimer();
    }
  }
}