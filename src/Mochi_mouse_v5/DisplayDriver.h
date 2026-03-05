#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <LovyanGFX.hpp>
#include "Settings.h"

class LGFX_Waveshare : public lgfx::LGFX_Device {
private:
  lgfx::Panel_ST7789  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;

public:
  // Solo la dichiarazione del costruttore, l'implementazione è nel .cpp!
  LGFX_Waveshare(); 
};

#endif // DISPLAY_DRIVER_H