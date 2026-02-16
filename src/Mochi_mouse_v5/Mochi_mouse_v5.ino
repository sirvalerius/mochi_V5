#include "USB.h"
#include "USBHIDMouse.h"
#include <math.h>

#include "Settings.h"
#include "DisplayDriver.h"
#include "MochiState.h"
#include "MochiView.h"
//#include "MochiServer.h" 
#include "MochiBLE.h"

// --- OGGETTI GLOBALI ---
LGFX_Waveshare display;
LGFX_Sprite canvas(&display);
USBHIDMouse Mouse;

MochiState mochi;
MochiView* view;
//MochiServer* webServer; // Puntatore al server
MochiBLE* ble;

Adafruit_NeoPixel statusLed(NUM_PIXELS, PIN_RGB, NEO_GRB + NEO_KHZ800);

void avantiPresentazione() {
  // Simula il tasto "Avanti" del mouse (Mouse 5)
  Mouse.click(0x10); 
}

void indietroPresentazione() {
  // Simula il tasto "Indietro" del mouse (Mouse 4)
  Mouse.click(0x08);
}

void setup() {
  Serial.begin(115200);
  USB.begin();
  Mouse.begin();

  statusLed.begin();
  statusLed.setBrightness(50); 
  statusLed.setPixelColor(0, statusLed.Color(0, 0, 0)); // Spegni all'inizio
  statusLed.show();

  mochi.begin();
  mochi.loadSettings();

  display.init();
  display.setRotation(1);
  pinMode(PIN_BL, OUTPUT); 
  digitalWrite(PIN_BL, HIGH); 
  
  canvas.createSprite(320, 172);
  
  view = new MochiView(&canvas);
  
  // Avviamo il Server Web passando il puntatore allo stato
  // webServer = new MochiServer(&mochi);
  // webServer->begin();

  ble = new MochiBLE(&mochi, &statusLed); 
  ble->begin();

  mochi.resetTimer();
}

void performMouseRoutine() {
  // Salti di gioia
  for(int j=0; j<2; j++) {
    for(int i=0; i<180; i+=10) {
      float jumpOffset = -abs(sin(i * M_PI / 180.0)) * 45;
      view->render(mochi, (int)jumpOffset, true, true);
      delay(5);
    }
  }

  // Movimento Mouse
  float r = 40.0;
  float lx = 40, ly = 0;
  for (int i = 0; i <= 360; i += 10) {
    float rad = i * M_PI / 180.0;
    float nx = r * cos(rad); float ny = r * sin(rad);
    Mouse.move((int)(nx - lx), (int)(ny - ly));
    lx = nx; ly = ny; 
    delay(8);
  }
}

void performDeathSequence() {
  // L'animazione dura circa 2 secondi
  for(int i = 0; i < 220; i += 3) {
    // Calcola offset Y: sale progressivamente (-i)
    // Aggiungiamo un tremolio orizzontale spettrale usando il seno
    int trembleX = (int)(sin(i / 8.0) * 6); 
    int offsetY = -i; // Sempre più in alto
    
    // Renderizza lo stato "isDying"
    // Passiamo false per wink e connected perché è un fantasma
    view->render(mochi, offsetY + trembleX, false, false); 
    delay(20); // Pausa per fluidità
  }

  // Animazione finita, è fuori schermo.
  // Ora eseguiamo il reset vero e proprio dei dati.
  mochi.finalizeDeath();
  
  // Opzionale: un momento di "vuoto" prima della rinascita
  canvas.fillScreen(K_WHITE);
  canvas.pushSprite(0,0);
  delay(800);
  
  // Reset del timer per non far scattare subito l'azione mouse
  mochi.resetTimer();
}

// --- NUOVA ROUTINE DI ANIMAZIONE CRESCITA ---
void performGrowthSequence() {
  AgeStage from = mochi.currentAge;
  AgeStage to = mochi.targetGrowthStage;

  // Animazione di circa 2.5 secondi
  int steps = 120;
  for(int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps; // Progresso da 0.0 a 1.0
    
    // Chiama la funzione speciale della vista
    view->drawGrowthFrame(t, from, to);
    delay(20);
  }

  // Flash bianco finale per "l'evoluzione"
  canvas.fillScreen(K_WHITE);
  canvas.pushSprite(0,0);
  delay(300);

  // Finalizza lo stato
  mochi.finalizeGrowth();
  mochi.resetTimer();
}

void loop() {
  unsigned long now = millis();
  bool isConnected = true;

  // 1. GESTIONE WEB SERVER (Importante!)
  // webServer->handle();

  // --- CONTROLLO MORTE ---
  // Se il server ha settato il flag isDying, fermiamo tutto ed eseguiamo l'animazione
  if (mochi.isDying) {
    performDeathSequence();
    // Dopo l'animazione, il flag è false e mochi è resettato.
    // Ritorniamo all'inizio del loop per ricominciare "puliti"
    return; 
  }

  // --- CONTROLLO MORTE ---
  if (mochi.needsGrowthAnimation) {
    performGrowthSequence();
    return;
  } 

  // 2. Aggiorna Logica
  mochi.updateDecay();
  
  // Se l'utente ha premuto un tasto web, mostriamo il cuore
  if (mochi.lastCommand != "") {
     // Se riceve un comando WiFi, forza l'attivazione immediata
     mochi.isHeartVisible = true;
     mochi.heartShowTime = millis();
     if (mochi.lastCommand == "prev") indietroPresentazione();
     else if (mochi.lastCommand == "next") avantiPresentazione();
     mochi.lastCommand = ""; // Reset del comando dopo averlo processato
  } else {
     // Altrimenti, usa la logica casuale standard
     mochi.triggerHeart(); 
  }

  // 3. Calcola Animazione
  float bounce = -abs(sin(fmod(now / 1000.0 * 2.5, M_PI))) * 12;
  bool wink = (fmod(now, 5000) < 200);

  // 4. Disegna
  view->render(mochi, (int)bounce, wink, isConnected);

  // 5. Routine Automatica (30 sec)
  if (mochi.timeForAction()) {
    mochi.recharge();
    performMouseRoutine();
    mochi.resetTimer();
  }
}