#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <LovyanGFX.hpp>
#include "Settings.h"
#include "Board.h"

class LGFX_Waveshare : public lgfx::LGFX_Device {
private:
  lgfx::Panel_ST7789  _panel_instance;  // JD9853 e' compatibile coi comandi ST7789
  lgfx::Bus_SPI       _bus_instance;

public:
  LGFX_Waveshare();

  // Configura bus + pannello in base alla board rilevata.
  // Va chiamata PRIMA di init(), dopo boardDetect().
  void configure(const BoardProfile& p);

  // Invia gamma/power specifici del JD9853 (board touch). Va chiamata DOPO
  // init()/setRotation(): l'init ST7789 di LovyanGFX non sblocca i comandi
  // estesi (0xDF) ne' setta la gamma del JD9853, da cui i colori scuri.
  void applyJD9853Tuning();
};

#endif // DISPLAY_DRIVER_H
