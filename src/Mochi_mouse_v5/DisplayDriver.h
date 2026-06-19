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
};

#endif // DISPLAY_DRIVER_H
