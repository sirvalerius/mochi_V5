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

void LGFX_Waveshare::applyJD9853Tuning() {
  // Sequenza gamma/power ufficiale del JD9853 (da CircuitPython/Waveshare).
  // Formato: comando, n_dati, dati... — NB: volutamente NON include i comandi
  // di geometria/formato/inversione/display-on (0x36, 0x3A, 0x2A/2B, 0x21,
  // 0x29, 0x35): quelli li gestisce LovyanGFX e non vanno sovrascritti.
  static const uint8_t seq[] = {
    0xDF, 2, 0x98, 0x53,                         // unlock comandi estesi
    0xB2, 1, 0x23,
    0xB7, 4, 0x00, 0x47, 0x00, 0x6F,
    0xBB, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0, // power
    0xC0, 2, 0x44, 0xA4,                         // power
    0xC1, 1, 0x16,
    0xC3, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
    0xC4, 12, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
    0xC8, 32, 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, // gamma
                0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
                0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
                0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
    0xD0, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
    0xD7, 2, 0x00, 0x30,
    0xE6, 1, 0x14,
    0xDE, 1, 0x01,                               // pagina 1
    0xB7, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
    0xC1, 3, 0x14, 0x15, 0xC0,
    0xC2, 2, 0x06, 0x3A,
    0xC4, 2, 0x72, 0x12,
    0xBE, 1, 0x00,
    0xDE, 1, 0x00,                               // ritorno pagina 0
  };

  startWrite();
  size_t i = 0;
  while (i < sizeof(seq)) {
    uint8_t cmd = seq[i++];
    uint8_t len = seq[i++];
    writeCommand(cmd);
    for (uint8_t j = 0; j < len; j++) writeData(seq[i++]);
  }
  endWrite();
}
