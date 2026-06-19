#include "Board.h"

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
  /* rotation    */ 1,
  /* hasStatusLed*/ true,
  /* rgbPin      */ 38
};

// --- Profilo: ESP32-S3-Touch-LCD-1.47 (JD9853 + AXS5106L) ---
// Il JD9853 ha lo scan di colonna invertito rispetto all'ST7789, quindi serve
// il mirror orizzontale: rotation 5 = rotation 1 + bit mirror in LovyanGFX.
static const BoardProfile PROFILE_JD9853 = {
  /* name        */ "ESP32-S3-Touch-LCD-1.47 (JD9853)",
  /* sclk        */ 38,
  /* mosi        */ 39,
  /* dc          */ 45,
  /* cs          */ 21,
  /* rst         */ 40,
  /* bl          */ 46,
  /* invert      */ false,
  /* rotation    */ 5,
  /* hasStatusLed*/ false,  // GPIO38 qui e' il clock LCD: nessun LED RGB
  /* rgbPin      */ -1
};

// Profilo attivo (default: la board storica, sovrascritto da boardDetect()).
BoardProfile g_board = PROFILE_ST7789;

// ================================================================
// PROBE I2C BIT-BANG
// ----------------------------------------------------------------
// NON usiamo la libreria Wire / il driver I2C di ESP-IDF: su una board dove
// questi GPIO NON sono un vero bus I2C (la variante ST7789, dove 41/42 sono
// DC/CS del display) il driver puo' fare abort() -> reboot loop. Toggolando i
// pin a mano restiamo nel puro GPIO, che non puo' mai crashare.
// ================================================================

static const int H_US = 10; // mezzo periodo di clock (~50 kHz: la velocita' non conta)

static inline void lineRelease(int pin) { pinMode(pin, INPUT_PULLUP); }      // rilascia: pull-up -> alto
static inline void lineLow(int pin)     { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }

// Invia START + indirizzo (write) e ritorna true se arriva l'ACK del device.
static bool i2cProbeBitbang(uint8_t addr7) {
  const int SDA = TOUCH_I2C_SDA;
  const int SCL = TOUCH_I2C_SCL;

  lineRelease(SDA);
  lineRelease(SCL);
  delayMicroseconds(20);

  // Il bus deve essere idle alto. Se una linea e' tenuta bassa non e' un bus
  // I2C sano: trattiamo come "non touch" senza nemmeno provare.
  if (digitalRead(SDA) == LOW || digitalRead(SCL) == LOW) return false;

  // START: SDA alto->basso mentre SCL e' alto.
  lineLow(SDA); delayMicroseconds(H_US);
  lineLow(SCL); delayMicroseconds(H_US);

  // Byte indirizzo (write = bit0 a 0), MSB per primo.
  uint8_t b = (uint8_t)(addr7 << 1);
  for (int i = 0; i < 8; i++) {
    if (b & 0x80) lineRelease(SDA); else lineLow(SDA);
    b <<= 1;
    delayMicroseconds(H_US);
    lineRelease(SCL); delayMicroseconds(H_US); // fronte di salita clock
    lineLow(SCL);     delayMicroseconds(H_US);
  }

  // Nono clock = ACK: rilasciamo SDA e leggiamo (basso = ACK).
  lineRelease(SDA);
  delayMicroseconds(H_US);
  lineRelease(SCL);
  delayMicroseconds(H_US);
  bool ack = (digitalRead(SDA) == LOW);
  lineLow(SCL);
  delayMicroseconds(H_US);

  // STOP: SDA basso->alto mentre SCL e' alto.
  lineLow(SDA);     delayMicroseconds(H_US);
  lineRelease(SCL); delayMicroseconds(H_US);
  lineRelease(SDA); delayMicroseconds(H_US);

  return ack;
}

static bool probeTouchController() {
  // Sblocca il chip touch dal reset.
  pinMode(TOUCH_RST_PIN, OUTPUT);
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(10);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(50);

  bool found = false;
  for (int i = 0; i < 3 && !found; i++) {
    if (i2cProbeBitbang(AXS5106L_ADDR)) found = true;
    else delay(5);
  }

  // Lascia i pin in alta impedenza: sulla board ST7789 sono DC/CS e l'init del
  // display li riconfigura subito dopo.
  pinMode(TOUCH_I2C_SDA, INPUT);
  pinMode(TOUCH_I2C_SCL, INPUT);
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
