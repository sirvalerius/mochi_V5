#ifndef MOCHI_BLE_H
#define MOCHI_BLE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>
#include "MochiState.h"

class MochiBLE {
private:
    MochiState* mochi;
    Adafruit_NeoPixel* statusLed;
    String bleName;
    BLEServer* pServer;
    BLECharacteristic* pCharacteristic; // Ripristinato il puntatore

public:
    MochiBLE(MochiState* m, Adafruit_NeoPixel* led);
    void begin();
    
    // Ripristinate le funzioni originali
    void scanForFriends();
    bool isConnected();
};

#endif // MOCHI_BLE_H