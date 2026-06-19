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
  // SOLO unlock + curva gamma del JD9853 (valori ufficiali Waveshare/CircuitPython).
  // ATTENZIONE: NON inviare i register gate/source (0xC3/0xC4), frame (0xB2) ne'
  // il blocco di pagina 1: quelli ridefiniscono la direzione di scansione e
  // l'ordine colore del pannello e vanno in conflitto con l'orientamento/inversione
  // gia' impostati da LovyanGFX (effetto: schermo specchiato + colori invertiti).
  // La gamma 0xC8 e' un puro rimappaggio di intensita': corregge i colori scuri
  // senza poter alterare mirror o inversione.
  static const uint8_t seq[] = {
    0xDF, 2, 0x98, 0x53,                         // unlock comandi estesi
    0xC8, 32, 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, // gamma
                0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
                0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
                0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
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
