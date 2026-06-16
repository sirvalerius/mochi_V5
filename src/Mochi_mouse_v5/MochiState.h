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

enum PendingAction {
  ACTION_NONE,
  ACTION_FEED,
  ACTION_PET,
  ACTION_TRAIN_STR,
  ACTION_TRAIN_SPD,
  ACTION_TRAIN_INT,
  ACTION_TRAIN_CHR
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
  int   statStr = 0;
  int   statSpd = 0;
  int   statInt = 0;
  int   statChr = 0;

  PendingAction pendingAction = ACTION_NONE;

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
  bool minigamePlayedThisSlot = false;

  bool isFriendNearby = false; // Almeno un altro Mochi è nei paraggi (discovery)

  // --- AMICI (persistiti) ---
  String friendIds[MAX_FRIENDS]; // ID degli amici (es. "Mochi-ABCD")
  int    friendCount = 0;

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

  // --- AMICI ---
  void   loadFriends();
  void   saveFriends();
  bool   addFriend(const String& id);
  bool   removeFriend(const String& id);
  bool   isFriend(const String& id);
  String getFriendsJson();

  // --- LOGICA ORARIO ---
  void syncTime(long unixTime);
  String getTimeString();
  time_t getNow();
  int getMissedIncrements(time_t newUnixTime);

  // --- LOGICA GIOCO ---
  void applyTick();
  void applyCommand(String cmd);
  void recharge();
  void queueAction(String action);
  void gainFromMinigame(PendingAction action, int score);
  String getStateJson();
  
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