#include "DisplayDriver.h"

// Il costruttore non tocca l'hardware: la configurazione vera avviene in
// configure(), chiamata dopo boardDetect() quando i pin sono noti.
LGFX_Waveshare::LGFX_Waveshare() {}

void LGFX_Waveshare::configure(const BoardProfile& p) {
  {
    auto cfg = _bus_instance.config();
    cfg.spi_host = SPI3_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.pin_sclk = p.sclk;
    cfg.pin_mosi = p.mosi;
    cfg.pin_dc   = p.dc;
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);
  }
  {
    auto cfg = _panel_instance.config();
    cfg.pin_cs   = p.cs;
    cfg.pin_rst  = p.rst;
    cfg.panel_width  = 172;
    cfg.panel_height = 320;
    cfg.offset_x = 34;
    cfg.offset_y = 0;
    cfg.invert   = p.invert;
    _panel_instance.config(cfg);
  }
  setPanel(&_panel_instance);
}
