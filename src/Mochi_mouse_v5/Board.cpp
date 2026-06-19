#include "Board.h"
#include <Wire.h>

// --- Pin del touch controller (solo sulla variante Touch) ---
// AXS5106L: I2C su SDA=42 / SCL=41, reset su 47, interrupt su 48.
#define TOUCH_I2C_SDA   42
#define TOUCH_I2C_SCL   41
#define TOUCH_RST_PIN   47
#define AXS5106L_ADDR   0x63

// --- Profilo: ESP32-S3-LCD-1.47 (ST7789, NON touch) — default storico ---
static const BoardProfile PROFILE_ST7789 = {
  /* name        */ "ESP32-S3-LCD-1.47 (ST7789)",
  /* sclk        */ 40,
  /* mosi        */ 45,
  /* dc          */ 41,
  /* cs          */ 42,
  /* rst         */ 39,
  /* bl          */ 48,
  /* invert      */ true,
  /* hasStatusLed*/ true,
  /* rgbPin      */ 38
};

// --- Profilo: ESP32-S3-Touch-LCD-1.47 (JD9853 + AXS5106L) ---
static const BoardProfile PROFILE_JD9853 = {
  /* name        */ "ESP32-S3-Touch-LCD-1.47 (JD9853)",
  /* sclk        */ 38,
  /* mosi        */ 39,
  /* dc          */ 45,
  /* cs          */ 21,
  /* rst         */ 40,
  /* bl          */ 46,
  /* invert      */ false,
  /* hasStatusLed*/ false,  // GPIO38 qui e' il clock LCD: nessun LED RGB
  /* rgbPin      */ -1
};

// Profilo attivo (default: la board storica, sovrascritto da boardDetect()).
BoardProfile g_board = PROFILE_ST7789;

// Porta il touch controller fuori dal reset e prova a contattarlo via I2C.
// Ritorna true se l'AXS5106L risponde (=> siamo sulla variante Touch).
static bool probeTouchController() {
  // Sblocca il chip touch dal reset.
  pinMode(TOUCH_RST_PIN, OUTPUT);
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(10);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(50);

  Wire.begin(TOUCH_I2C_SDA, TOUCH_I2C_SCL);
  Wire.setClock(100000);

  // Un ACK valido all'indirizzo dell'AXS5106L identifica la board touch.
  // Sulla board ST7789 questi GPIO sono DC/CS del display (ancora non
  // inizializzato): nessuno risponde all'indirizzo I2C, quindi endTransmission != 0.
  bool found = false;
  for (int i = 0; i < 3 && !found; i++) {
    Wire.beginTransmission(AXS5106L_ADDR);
    if (Wire.endTransmission() == 0) found = true;
    else delay(5);
  }

  Wire.end();
  return found;
}

void boardDetect() {
  if (probeTouchController()) {
    g_board = PROFILE_JD9853;
  } else {
    g_board = PROFILE_ST7789;
  }
  Serial.print("[BOARD] Rilevata: ");
  Serial.println(g_board.name);
}
