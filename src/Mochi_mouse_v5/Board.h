#ifndef BOARD_H
#define BOARD_H

#include <Arduino.h>

// ================================================================
// PROFILO HARDWARE (runtime)
// ----------------------------------------------------------------
// Esistono due varianti Waveshare LCD 1.47 con la stessa risoluzione
// (172x320) ma pin e controller diversi:
//
//   - ESP32-S3-LCD-1.47        -> controller ST7789, ha un LED RGB su GPIO38
//   - ESP32-S3-Touch-LCD-1.47  -> controller JD9853 (compatibile coi comandi
//                                 ST7789) + touch AXS5106L su I2C. Qui GPIO38
//                                 e' il CLOCK del display, quindi NIENTE LED RGB.
//
// Il firmware rileva la board al boot (probe I2C del touch) e popola g_board.
// ================================================================

struct BoardProfile {
  const char* name;
  // --- Display SPI ---
  int  sclk;
  int  mosi;
  int  dc;
  int  cs;
  int  rst;
  int  bl;        // backlight (PWM, attivo alto su entrambe)
  bool invert;    // ST7789 vuole invert=true, JD9853 invert=false
  int  rotation;  // LovyanGFX setRotation: 1 = landscape; +4 = mirror orizzontale
  // --- LED RGB di stato ---
  bool hasStatusLed;
  int  rgbPin;
  // --- Tuning post-init ---
  bool jd9853Tuning; // invia gamma/power JD9853 dopo init (solo board touch)
};

// Profilo attivo, popolato da boardDetect() in setup() PRIMA di init del display.
extern BoardProfile g_board;

// Rileva la variante hardware e imposta g_board. Va chiamata per prima cosa,
// prima di configurare il display o il LED.
void boardDetect();

#endif // BOARD_H
