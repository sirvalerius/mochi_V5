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

public:
  float hunger;
  float happy;
  unsigned long lastActionTime = 0;

  String remoteTime  = "00/00/0000 00:00";
  
  bool isDying = false;
  bool isHeartVisible = false;
  unsigned long heartShowTime = 0;

  bool isBubbleVisible = false;
  char bubbleType = '!'; // '!' o '?'
  unsigned long bubbleShowTime = 0;

  String lastCommand = ""; 
  unsigned long commandFeedbackTime = 0;

  AgeStage currentAge = ADULT; // Partiamo da adulto come default
  bool needsGrowthAnimation = false; // Variabile per segnalare al main che serve una transizione
  AgeStage targetGrowthStage;

  MochiState() {
  }

  void begin() {
    lastActionTime = millis();
    loadState(); // Chiamata qui, quando l'hardware è pronto
  }

  // --- LOGICA MEMORIA ---
  void loadState() {
    if (prefs.begin("mochi-data", false)) {
      hunger = prefs.getFloat("hunger", 50.0f);
      happy = prefs.getFloat("happy", 50.0f);
      currentAge = (AgeStage)prefs.getInt("age", (int)ADULT);
      remoteTime = prefs.getString("savedTime", "00/00/0000 00:00");
      prefs.end();
      Serial.println("Dati caricati correttamente!");
    } else {
      Serial.println("Errore apertura NVS!");
      hunger = 50.0; happy = 50.0; remoteTime = "00/00/0000 00:00";
    }
  }

  void saveState() {
    prefs.begin("mochi-data", false);
    prefs.putFloat("hunger", hunger);
    prefs.putFloat("happy", happy);
    prefs.putInt("age", (int)currentAge);
    prefs.putString("savedTime", remoteTime);
    prefs.end();
    Serial.println("Dati salvati in memoria!");
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
    if (cmd.startsWith("time:")) {
        // Estraiamo la stringa dell'ora (tutto ciò che viene dopo "time:")
        remoteTime = cmd.substring(5);
        Serial.println("Ora sincronizzata: " + remoteTime);
        
        // Qui puoi salvare remoteTime in una variabile per mostrarla a video
        // mochiTime = remoteTime; 
    } else if (cmd == "FEED") hunger += 20.0; 
    else if (cmd == "PLAY") happy += 20.0;
    else if (cmd == "GROW") growUp();
    else if (cmd == "KILL") {
      isDying = true;
      return;
    } else if (cmd == "GROW") {
       growUp();
       return; 
    } else if (cmd = "prev") {} 
    else if (cmd = "next") {}

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