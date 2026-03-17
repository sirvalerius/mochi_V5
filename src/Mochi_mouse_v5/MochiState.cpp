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

    applySettings();
}

void MochiState::saveSettings(String newJson) {
    settingsBlob = newJson;
    prefs.begin("mochi-data", false);
    prefs.putString("settings", settingsBlob);
    prefs.end();

    applySettings();
}

void MochiState::applySettings() {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, settingsBlob);
  
  if (!error) {
    // 1. Applica la Luminosità direttamente al Pin della Retroilluminazione (PIN_BL)
    if (doc.containsKey("brightness")) {
      screenBrightness = doc["brightness"];
      analogWrite(PIN_BL, screenBrightness); // Applica fisicamente la luminosità al display!
    }
    
    // 2. Estrai i colori, convertili e avvisa che sono cambiati
    if (doc.containsKey("bgTop") && doc.containsKey("bgBottom")) {
      uint16_t newTop = hexToRGB565(doc["bgTop"]);
      uint16_t newBottom = hexToRGB565(doc["bgBottom"]);
      
      if (bgTopColor != newTop || bgBottomColor != newBottom) {
        bgTopColor = newTop;
        bgBottomColor = newBottom;
        colorsUpdated = true; // Segnala che i colori sono nuovi!
      }
    }
  }
}

// ==========================================
// LOGICA ORARIO
// ==========================================

void MochiState::syncTime(long unixTime) {  
  int missedTicks = getMissedIncrements(unixTime); //calcola tick di tempo passati dall'ultimo momento in cui il device era acceso  
  if (missedTicks > 0) {
      Serial.print("Mochi è stato spento per ");
      Serial.print(missedTicks * 5);
      Serial.println(" minuti!");
      
      // Qui in futuro potrai abbassare la fame (hunger) o la felicità (happy) 
      // in base a 'missedTicks', così quando lo riaccendi avrà fame!
      // Esempio: hunger -= (missedTicks * 0.5); 
  }

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
  
  // Usiamo gmtime per visualizzare il timestamp 
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

void MochiState::applyTick() {
  // 1. Aggiorna i bisogni (Fame, Felicità, ecc.)
  updateDecay();
  recharge();
  // 2. Controlla l'orologio biologico (Nascita, Evoluzione, Morte)
  checkLifecycle();
  // 3. Salva i progressi in memoria in modo sicuro ogni 5 minuti
  saveState();
  
  // Debug
  Serial.println("--- TICK 5 MINUTI ESEGUITO ---");
  Serial.printf("Fame: %.1f | Felicità: %.1f | Età: %d\n", hunger, happy, currentAge);
}

void MochiState::updateDecay() {
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
}

void MochiState::recharge() {
  hunger += 10.0; if (hunger > MAX_VAL) hunger = MAX_VAL;
  happy += 5.0;  if (happy > MAX_VAL) happy = MAX_VAL;
  saveState(); // SALVA dopo la routine automatica dei 30s
}

// ==========================================
// CONDIZIONI DI EVOLUZIONE (Helpers)
// ==========================================

/*
// Lunedì dopo le 10:00 (oppure i giorni successivi)
bool MochiState::shouldHatch(int day, int hour) {
    return (day > 1 || (day == 1 && hour >= 10));
}

// Martedì dopo le 18:00 (oppure i giorni successivi)
bool MochiState::shouldBecomeAdult(int day, int hour) {
    return (day > 2 || (day == 2 && hour >= 18));
}

// Giovedì dopo le 18:00 (oppure i giorni successivi)
bool MochiState::shouldBecomeElder(int day, int hour) {
    return (day > 4 || (day == 4 && hour >= 18));
}

// Venerdì dopo le 18:00 (oppure nel weekend: day 0 è Domenica, >5 è Sabato)
bool MochiState::shouldDie(int day, int hour) {
    return (day == 0 || day > 5 || (day == 5 && hour >= 18));
} */

AgeStage MochiState::getExpectedStage(int day, int hour) {
    // 1. IL WEEKEND (Morte/Uovo): Da Venerdì ore 18:00 a Lunedì ore 09:59
    if (day == 6 || day == 0) return EGG; // Sabato e Domenica
    if (day == 5 && hour >= 18) return EGG; // Venerdì sera
    if (day == 1 && hour < 10) return EGG; // Lunedì mattina presto

    // 2. FASE BABY: Da Lunedì ore 10:00 a Martedì ore 17:59
    if (day == 1 && hour >= 10) return BABY;
    if (day == 2 && hour < 18) return BABY;

    // 3. FASE ADULTO: Da Martedì ore 18:00 a Giovedì ore 17:59
    if (day == 2 && hour >= 18) return ADULT;
    if (day == 3) return ADULT; // Mercoledì tutto il giorno
    if (day == 4 && hour < 18) return ADULT;

    // 4. FASE ANZIANO: Da Giovedì ore 18:00 a Venerdì ore 17:59
    if (day == 4 && hour >= 18) return ELDER;
    if (day == 5 && hour < 18) return ELDER;

    return EGG; // Sicurezza fallback
}

void MochiState::checkLifecycle() {
    time_t now = getNow();
    if (now == 0 || needsGrowthAnimation || isDying || (millis() - lastEvolutionTime < evolutionCooldown)) return; // Se l'ora non è sincro o sta già animando, esci

    struct tm * timeinfo;
    timeinfo = gmtime(&now); // Converte il timestamp in ore/giorni
    
    int day = timeinfo->tm_wday; // 0 = Domenica, 1 = Lunedì, 2 = Martedì... 5 = Venerdì
    int hour = timeinfo->tm_hour;

    AgeStage expected = getExpectedStage(day, hour);
    
    if ((expected == EGG && currentAge != EGG) || currentAge > expected) {
        isDying = true;
        targetGrowthStage = EGG;
        return;
    }

    // Se è tutto ok o è indietro con l'età, facciamo la scalata CATCH-UP (1 step per ogni tick)
    if (currentAge == EGG && expected >= BABY) {
        targetGrowthStage = BABY;
        needsGrowthAnimation = true;
    } 
    else if (currentAge == BABY && expected >= ADULT) {
        targetGrowthStage = ADULT;
        needsGrowthAnimation = true;
    }
    else if (currentAge == ADULT && expected == ELDER) {
        targetGrowthStage = ELDER;
        needsGrowthAnimation = true;
    }
}

int MochiState::getMissedIncrements(time_t newUnixTime) {
    // 1. Se è la prima accensione in assoluto o l'orario salvato era il default (1700000000)
    if (baseUnixTime == 0 || baseUnixTime == 1700000000) {
        return 0; // Nessun incremento perso, si parte da zero
    }

    // 2. Sicurezza: Se per caso il nuovo orario è indietro rispetto al passato (errore di rete)
    if (newUnixTime <= baseUnixTime) {
        return 0;
    }

    // 3. Calcolo della differenza in secondi
    unsigned long diffSeconds = newUnixTime - baseUnixTime;

    // 4. Dividiamo per 300 (i secondi in 5 minuti)
    // Se ACTION_INTERVAL lo avevi impostato a 300000 (ms), corrisponde esattamente a 300s.
    int missedIntervals = diffSeconds / 300; 

    return missedIntervals;
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

// Funzione helper per convertire colore Web (#FFA0C8) a colore Display (RGB565)
uint16_t hexToRGB565(const char* hexStr) {
    if (hexStr[0] == '#') hexStr++;
    long rgb = strtol(hexStr, NULL, 16);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}