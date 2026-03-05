#ifndef MOCHI_BLE_H
#define MOCHI_BLE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>
#include "MochiState.h"

// Rimosse le macro UUID e le classi Callback (ora sono sicure nel .cpp)

class MochiBLE {
private:
    MochiState* mochi;
    Adafruit_NeoPixel* statusLed;
    String bleName;
    BLEServer* pServer;

public:
    MochiBLE(MochiState* m, Adafruit_NeoPixel* led);
    void begin();
};

#endif // MOCHI_BLE_H