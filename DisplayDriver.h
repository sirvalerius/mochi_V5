#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <LovyanGFX.hpp>
#include "Settings.h"

class LGFX_Waveshare : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
public:
  LGFX_Waveshare() {
    { 
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI3_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000; 
      cfg.pin_sclk = PIN_SCLK; 
      cfg.pin_mosi = PIN_MOSI; 
      cfg.pin_dc   = PIN_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    { 
      auto cfg = _panel_instance.config();
      cfg.pin_cs = PIN_CS; 
      cfg.pin_rst = PIN_RST;
      cfg.panel_width = 172; 
      cfg.panel_height = 320;
      cfg.offset_x = 34; 
      cfg.offset_y = 0;
      cfg.invert = true; 
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

#endif