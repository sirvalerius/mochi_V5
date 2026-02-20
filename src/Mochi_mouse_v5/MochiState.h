#ifndef MOCHI_STATE_H
#define MOCHI_STATE_H

#include <Preferences.h> // Libreria per la memoria permanente
#include "Settings.h"

enum AgeStage {
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

  AgeStage currentAge = ADULT; // Partiamo da adulto come default
  bool needsGrowthAnimation = false; // Variabile per segnalare al main che serve una transizione
  AgeStage targetGrowthStage;

  MochiState() {
  }

  void begin() {
    lastActionTime = millis();
    loadState();    // Loads hunger, happyness, etc
    loadSettings(); // Loads Json Settings
  }

  // --- LOGICA MEMORIA ---
  void loadState() {
    if (prefs.begin("mochi-data", false)) {
      hunger = prefs.getFloat("hunger", 50.0f);
      happy = prefs.getFloat("happy", 50.0f);
      currentAge = (AgeStage)prefs.getInt("age", (int)ADULT);
      baseUnixTime = prefs.getULong("savedTime", 1700000000); // 1700... è un default recente
      syncMillis = millis(); // Ripartiamo a contare da qui
      prefs.end();
      Serial.println("Dati caricati correttamente!");
    } else {
      Serial.println("Errore apertura NVS!");
      hunger = 50.0; happy = 50.0;
    }
  }

  void saveState() {
    prefs.begin("mochi-data", false);
    prefs.putFloat("hunger", hunger);
    prefs.putFloat("happy", happy);
    prefs.putInt("age", (int)currentAge);
    unsigned long currentUnix = baseUnixTime + ((millis() - syncMillis) / 1000);
    prefs.putULong("savedTime", currentUnix);
    prefs.end();
    Serial.println("Dati salvati in memoria!");
  }

  void loadSettings() {
      prefs.begin("mochi-data", false);
      // Legge la stringa "settings", se non esiste usa "{}"
      settingsBlob = prefs.getString("settings", "{}");
      prefs.end();
  }

  void saveSettings(String newJson) {
      settingsBlob = newJson;
      prefs.begin("mochi-data", false);
      prefs.putString("settings", settingsBlob);
      prefs.end();
  }

  // --- LOGICA ORARIO ---
  void syncTime(long unixTime) {
    baseUnixTime = unixTime;
    syncMillis = millis();
    Serial.printf("Ora Sincronizzata: %ld\n", unixTime);
  }

  // Metodo per ottenere l'ora HH:MM aggiornata
  String getTimeString() {
    if (baseUnixTime == 0) return "--:--";
    
    // Calcoliamo i secondi passati dalla sincronizzazione
    unsigned long elapsedSeconds = (millis() - syncMillis) / 1000;
    time_t now = baseUnixTime + elapsedSeconds;
    
    // Usiamo gmtime per visualizzare il timestamp "così com'è" 
    // senza che l'ESP32 aggiunga ore di testa sua
    struct tm * timeinfo;
    timeinfo = gmtime(&now); 
    
    char buffer[6]; // "HH:MM\0"
    sprintf(buffer, "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
    return String(buffer);
}

  // Restituisce il timestamp Unix attuale "calcolato"
  time_t getNow() {
      if (baseUnixTime == 0) return 0; 
      return baseUnixTime + (millis() - syncMillis) / 1000;
  }

  // --- LOGICA GIOCO ---
  void updateDecay() {
    hunger -= HUNGER_DECAY;
    happy -= HAPPY_DECAY;
    if (hunger < 0) hunger = 0;
    if (happy < 0) happy = 0;
    
    if (lastCommand != "" && millis() - commandFeedbackTime > 1000) {
      lastCommand = "";
    }
  }

  void applyCommand(String cmd) {
    lastCommand = cmd;
    commandFeedbackTime = millis();
    if (cmd == "FEED") hunger += 20.0; 
    else if (cmd == "PLAY") happy += 20.0;
    else if (cmd == "GROW") growUp();
    else if (cmd == "KILL") {
      isDying = true;
      return;
    } else if (cmd == "GROW") {
      growUp();
      return; 
    } else if (cmd == "prev") {
      toggleAutoclick();
    } 
    else if (cmd == "next") {
      toggleAutoclick();
    }

    if (hunger > MAX_VAL) hunger = MAX_VAL;
    if (happy > MAX_VAL) happy = MAX_VAL;

    saveState();
    triggerHeart();
  }

  void recharge() {
    hunger += 10.0; if (hunger > MAX_VAL) hunger = MAX_VAL;
    happy += 5.0;  if (happy > MAX_VAL) happy = MAX_VAL;
    saveState(); // SALVA dopo la routine automatica dei 30s
  }

  void triggerHeart() {
    // Solo logica casuale: se non è già visibile, c'è una piccola chance
    if (!isHeartVisible && random(5000) < 5) { // Ho abbassato un po' la probabilità
      isHeartVisible = true;
      heartShowTime = millis();
    }
    
    // Gestione dello spegnimento del cuore dopo 2.5 secondi
    if (isHeartVisible && (millis() - heartShowTime > 2500)) {
      isHeartVisible = false;
    }
  }

  void triggerBubble(char type) {
    isBubbleVisible = true;
    bubbleType = type;
    bubbleShowTime = millis();
  }

  // Nel metodo updateDecay() o simile, aggiungi il check per lo spegnimento
  void updateBubbles() {
      if (isBubbleVisible && (millis() - bubbleShowTime > 2500)) {
          isBubbleVisible = false;
      }
  }

  float getProgress() {
    float p = (float)(millis() - lastActionTime) / (float)ACTION_INTERVAL;
    return (p > 1.0) ? 1.0 : p;
  }

  bool timeForAction() {
    return (millis() - lastActionTime > ACTION_INTERVAL);
  }

  void resetTimer() {
    lastActionTime = millis();
  }

  void toggleAutoclick() {
    isAutoClickActive = !isAutoClickActive;
    isBubbleVisible = isAutoClickActive;
  }

  void growUp() {
    if (currentAge == BABY) {
      targetGrowthStage = ADULT;
      needsGrowthAnimation = true;
    } else if (currentAge == ADULT) {
      targetGrowthStage = ELDER;
      needsGrowthAnimation = true;
    } else if (currentAge == ELDER) {
      isDying = true;
      targetGrowthStage = BABY;
    }
    // Se è ELDER non cresce più
  }

  void finalizeGrowth() {
    currentAge = targetGrowthStage;
    needsGrowthAnimation = false;
    saveState();
  }

  void finalizeDeath() {
    killMochi(); // Svuota la flash
    hunger = 10.0; 
    happy = 10.0;
    currentAge = BABY;
    saveState(); // Salva i valori di rinascita
    isDying = false; // Finito! Torna normale
  }

  void killMochi() {
    prefs.begin("mochi-data", false);
    prefs.clear();
    prefs.end();
    Serial.println("Memoria Mochi resettata!");
  }
};

#endif