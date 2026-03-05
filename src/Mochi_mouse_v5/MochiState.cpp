#include "MochiState.h"

MochiState::MochiState() {
}

void MochiState::begin() {
  lastActionTime = millis();
  loadState();    // Loads hunger, happyness, etc
  loadSettings(); // Loads Json Settings
}

// ==========================================
// LOGICA MEMORIA
// ==========================================

void MochiState::loadState() {
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

void MochiState::saveState() {
  prefs.begin("mochi-data", false);
  prefs.putFloat("hunger", hunger);
  prefs.putFloat("happy", happy);
  prefs.putInt("age", (int)currentAge);
  unsigned long currentUnix = baseUnixTime + ((millis() - syncMillis) / 1000);
  prefs.putULong("savedTime", currentUnix);
  prefs.putString("settings", settingsBlob);
  prefs.end();
  Serial.println("Dati salvati in memoria!");
}

void MochiState::loadSettings() {
    prefs.begin("mochi-data", false);
    // Legge la stringa "settings", se non esiste usa "{}"
    settingsBlob = prefs.getString("settings", "{}");
    prefs.end();
}

void MochiState::saveSettings(String newJson) {
    settingsBlob = newJson;
    prefs.begin("mochi-data", false);
    prefs.putString("settings", settingsBlob);
    prefs.end();
}

// ==========================================
// LOGICA ORARIO
// ==========================================

void MochiState::syncTime(long unixTime) {
  baseUnixTime = unixTime;
  syncMillis = millis();
  Serial.printf("Ora Sincronizzata: %ld\n", unixTime);
  saveState();
}

String MochiState::getTimeString() {
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

time_t MochiState::getNow() {
    if (baseUnixTime == 0) return 0; 
    return baseUnixTime + (millis() - syncMillis) / 1000;
}

// ==========================================
// LOGICA GIOCO
// ==========================================

void MochiState::updateDecay() {
  hunger -= HUNGER_DECAY;
  happy -= HAPPY_DECAY;
  if (hunger < 0) hunger = 0;
  if (happy < 0) happy = 0;

  if (currentAge != EGG) {
    hunger -= HUNGER_DECAY;
    happy -= HAPPY_DECAY;
    if (hunger < 0) hunger = 0;
    if (happy < 0) happy = 0;
  }
  
  // 2. Pulizia dell'ultimo comando
  if (lastCommand != "" && millis() - commandFeedbackTime > 1000) {
    lastCommand = "";
  }
  
  if (lastCommand != "" && millis() - commandFeedbackTime > 1000) {
    lastCommand = "";
  }
}

void MochiState::checkLifecycle() {
    time_t now = getNow();
    if (now == 0 || needsGrowthAnimation || isDying) return; // Se l'ora non è sincro o sta già animando, esci

    struct tm * timeinfo;
    timeinfo = gmtime(&now); // Converte il timestamp in ore/giorni
    
    int day = timeinfo->tm_wday; // 0 = Domenica, 1 = Lunedì, 2 = Martedì... 5 = Venerdì
    int hour = timeinfo->tm_hour;
    
    // Lunedì (1) ore 10: Uovo -> Baby
    if (currentAge == EGG && day == 1 && hour >= 10) {
        targetGrowthStage = BABY;
        needsGrowthAnimation = true;
    } 
    // Martedì (2) ore 18: Baby -> Adulto
    else if (currentAge == BABY && day == 2 && hour >= 18) {
        targetGrowthStage = ADULT;
        needsGrowthAnimation = true;
    }
    // Giovedì (4) ore 18: Adulto -> Vecchio
    else if (currentAge == ADULT && day == 4 && hour >= 18) {
        targetGrowthStage = ELDER;
        needsGrowthAnimation = true;
    }
    // Venerdì (5) ore 18: Vecchio -> Muore / Torna Uovo
    else if (currentAge == ELDER && day == 5 && hour >= 18) {
        // Puoi scegliere se farlo morire (isDying = true) o farlo tornare direttamente uovo
        isDying = true; 
        targetGrowthStage = EGG; 
    }
}

void MochiState::applyCommand(String cmd) {
  lastCommand = cmd;
  commandFeedbackTime = millis();
  if (cmd == "FEED") hunger += 20.0; 
  else if (cmd == "PLAY") happy += 20.0;
  else if (cmd == "KILL") {
    isDying = true;
    return;
  } else if (cmd == "GROW") {
    growUp();
    return; 
  } else if (cmd == "prev" || cmd == "next") {
    toggleAutoclick();
  }

  if (hunger > MAX_VAL) hunger = MAX_VAL;
  if (happy > MAX_VAL) happy = MAX_VAL;

  saveState();
  triggerHeart();
}

void MochiState::recharge() {
  hunger += 10.0; if (hunger > MAX_VAL) hunger = MAX_VAL;
  happy += 5.0;  if (happy > MAX_VAL) happy = MAX_VAL;
  saveState(); // SALVA dopo la routine automatica dei 30s
}

void MochiState::triggerHeart() {
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

void MochiState::triggerBubble(char type) {
  isBubbleVisible = true;
  bubbleType = type;
  bubbleShowTime = millis();
}

void MochiState::updateBubbles() {
    if (isBubbleVisible && (millis() - bubbleShowTime > 2500)) {
        isBubbleVisible = false;
    }
}

float MochiState::getProgress() {
  float p = (float)(millis() - lastActionTime) / (float)ACTION_INTERVAL;
  return (p > 1.0) ? 1.0 : p;
}

bool MochiState::timeForAction() {
  return (millis() - lastActionTime > ACTION_INTERVAL);
}

void MochiState::resetTimer() {
  lastActionTime = millis();
}

void MochiState::toggleAutoclick() {
  isAutoClickActive = !isAutoClickActive;
  isBubbleVisible = isAutoClickActive;
}

void MochiState::growUp() {
  // Aggiunto il salto UOVO -> CUCCIOLO
  if (currentAge == EGG) {
    targetGrowthStage = BABY;
    needsGrowthAnimation = true;
  } 
  else if (currentAge == BABY) {
    targetGrowthStage = ADULT;
    needsGrowthAnimation = true;
  } 
  else if (currentAge == ADULT) {
    targetGrowthStage = ELDER;
    needsGrowthAnimation = true;
  } 
  else if (currentAge == ELDER) {
    isDying = true;
    targetGrowthStage = EGG; // Dopo la morte diventerà un uovo
  }
}

void MochiState::finalizeGrowth() {
  currentAge = targetGrowthStage;
  needsGrowthAnimation = false;
  saveState();
}

void MochiState::finalizeDeath() {
  killMochi(); // Svuota la flash
 
  hunger = 50.0; 
  happy = 50.0;
  currentAge = EGG; 
  
  saveState(); // Salva i valori di rinascita
  isDying = false; // Finito! Torna normale
}

void MochiState::killMochi() {
  prefs.begin("mochi-data", false);
  prefs.clear();
  prefs.end();
  Serial.println("Memoria Mochi resettata!");
}