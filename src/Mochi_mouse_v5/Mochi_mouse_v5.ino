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
  if (sysState == STATE_NORMAL) return;

  // 1. SALTI DI GIOIA
  if (sysState == STATE_JUMPING) {
    if (now - animStartTime >= 5) { 
      animStartTime = now;
      int i = (animStep % 18) * 10; 
      float jumpOffset = -abs(sin(i * M_PI / 180.0)) * 45;
      view->render(mochi, (int)jumpOffset, true, true);
      
      animStep++;
      if (animStep >= 36) { 
        sysState = STATE_MOVING_MOUSE; 
        animStep = 0;
        animLx = 40; animLy = 0;
      }
    }
  } 
  // 2. MOVIMENTO DEL MOUSE
  else if (sysState == STATE_MOVING_MOUSE) {
    if (now - animStartTime >= 8) { 
      animStartTime = now;
      int i = animStep * 10;
      float rad = i * M_PI / 180.0;
      float nx = 40.0 * cos(rad); 
      float ny = 40.0 * sin(rad);
      Mouse.move((int)(nx - animLx), (int)(ny - animLy));
      animLx = nx; animLy = ny;
      
      animStep++;
      if (animStep > 36) { 
        sysState = STATE_NORMAL; 
        mochi.resetTimer();
      }
    }
  }
  // 3. ANIMAZIONE MORTE
  else if (sysState == STATE_DYING) {
    if (now - animStartTime >= 20) { 
      animStartTime = now;
      int i = animStep * 3;
      int trembleX = (int)(sin(i / 8.0) * 6);
      int offsetY = -i;
      view->render(mochi, offsetY + trembleX, false, false);
      
      animStep++;
      if (i >= 220) { 
        mochi.finalizeDeath(); 
        canvas.fillScreen(K_WHITE);
        canvas.pushSprite(0,0);
        sysState = STATE_DEAD_PAUSE; 
        animStartTime = now;
      }
    }
  }
  // 4. PAUSA BIANCA POST-MORTE
  else if (sysState == STATE_DEAD_PAUSE) {
    if (now - animStartTime >= 800) { 
      mochi.resetTimer();
      sysState = STATE_NORMAL; 
    }
  }
  // 5. ANIMAZIONE CRESCITA
  else if (sysState == STATE_GROWING) {
    if (now - animStartTime >= 20) { 
      animStartTime = now;
      float t = (float)animStep / 120.0; 
      // Assicurati che drawGrowthFrame esista in MochiView. Se usavi un altro metodo, aggiorna qui.
      // view->drawGrowthFrame(t, mochi.currentAge, mochi.targetGrowthStage);
      
      animStep++;
      if (animStep > 120) { 
        canvas.fillScreen(K_WHITE);
        canvas.pushSprite(0,0);
        sysState = STATE_GROWING_FLASH; 
        animStartTime = now;
      }
    }
  }
  // 6. FLASH BIANCO POST-CRESCITA
  else if (sysState == STATE_GROWING_FLASH) {
    if (now - animStartTime >= 300) { 
      mochi.finalizeGrowth();
      mochi.resetTimer();
      sysState = STATE_NORMAL; 
    }
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

  // 1. GESTIONE SERVER (Decommenta se usi WiFi)
  // if (webServer) webServer->handle(); 

  // 2. GESTIONE ANIMAZIONI (Uscita anticipata se in corso)
  if (sysState != STATE_NORMAL) {
    handleAnimations(now);
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
      sysState = STATE_JUMPING;
      animStep = 0;
      animStartTime = now;
    } else {
      mochi.resetTimer();
    }
  }
}