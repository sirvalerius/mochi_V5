#ifndef MOCHI_STATE_H
#define MOCHI_STATE_H

#include <Preferences.h> // Libreria per la memoria permanente
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

  time_t baseUnixTime = 0;       // Il tempo Unix ricevuto via BLE/WiFi
  unsigned long syncMillis = 0;  // Il valore di millis() al momento della sicro

public:
  float hunger;
  float happy;
  unsigned long lastActionTime = 0;

  String settingsBlob = "{}"; // Default: JSON vuoto
  
  bool isDying = false;
  bool isHeartVisible = false;
  unsigned long heartShowTime = 0;

  bool isBubbleVisible = false;
  char bubbleType = '!'; // '!' o '?'
  unsigned long bubbleShowTime = 0;

  bool isAutoClickActive = false;

  String lastCommand = ""; 
  unsigned long commandFeedbackTime = 0;

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

  // --- LOGICA ORARIO ---
  void syncTime(long unixTime);
  String getTimeString();
  time_t getNow();

  // --- LOGICA GIOCO ---
  void updateDecay();
  void checkLifecycle();
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