#ifndef MOCHI_STATE_H
#define MOCHI_STATE_H

#include <Preferences.h> // Libreria per la memoria permanente
#include <ArduinoJson.h>
#include "Settings.h"

enum AgeStage {
  EGG,
  BABY,
  ADULT,
  ELDER
};

class MochiState {
private:
  Preferences prefs; // Oggetto per gestire i salvataggi

  int evolutionCooldown = STATE_COOLDOWN;

/*  bool shouldHatch(int day, int hour);
  bool shouldBecomeAdult(int day, int hour);
  bool shouldBecomeElder(int day, int hour);
  bool shouldDie(int day, int hour);
*/
  AgeStage getExpectedStage(int day, int hour);

  time_t baseUnixTime = 0;       // Il tempo Unix ricevuto via BLE/WiFi
  unsigned long syncMillis = 0;  // Il valore di millis() al momento della sicro

  // --- LOGICA GIOCO ---
  void updateDecay();
  void checkLifecycle();

public:
  float hunger;
  float happy;
  unsigned long lastActionTime = 0;

  String settingsBlob = "{}"; // Default: JSON vuoto

  uint16_t bgTopColor = K_BG_TOP;    // Default: K_BG_TOP
  uint16_t bgBottomColor = K_BG_BOTTOM; // Default: K_BG_BOTTOM
  bool colorsUpdated = false;

  int screenBrightness = 255;
  
  bool isDying = false;
  bool isHeartVisible = false;
  unsigned long heartShowTime = 0;

  bool isBubbleVisible = false;
  char bubbleType = '!'; // '!' o '?'
  unsigned long bubbleShowTime = 0;

  bool isAutoClickActive = false;

  String lastCommand = ""; 
  unsigned long commandFeedbackTime = 0;

  unsigned long lastEvolutionTime = 0;

  AgeStage currentAge = EGG; // Partiamo da adulto come default
  bool needsGrowthAnimation = false; // Variabile per segnalare al main che serve una transizione
  AgeStage targetGrowthStage;

  // --- COSTRUTTORE ---
  MochiState();

  // --- METODI PRINCIPALI ---
  void begin();

  // --- LOGICA MEMORIA ---
  void loadState();
  void saveState();
  void loadSettings();
  void saveSettings(String newJson);

  void applySettings();            // Nuova funzione per applicare il JSON dei setting

  // --- LOGICA ORARIO ---
  void syncTime(long unixTime);
  String getTimeString();
  time_t getNow();
  int getMissedIncrements(time_t newUnixTime);

  // --- LOGICA GIOCO ---
  void applyTick();
  void applyCommand(String cmd);
  void recharge();
  
  // --- GESTIONE EFFETTI VISIVI ---
  void triggerHeart();
  void triggerBubble(char type);
  void updateBubbles();

  // --- GESTIONE TIMER E AZIONI ---
  float getProgress();
  bool timeForAction();
  void resetTimer();
  void toggleAutoclick();

  // --- GESTIONE CICLO VITALE ---
  void growUp();
  void finalizeGrowth();
  void finalizeDeath();
  void killMochi();
};

#endif