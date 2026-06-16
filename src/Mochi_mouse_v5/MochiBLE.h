#ifndef MOCHI_BLE_H
#define MOCHI_BLE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>
#include "MochiState.h"

class MochiNow; // Forward declaration (discovery/visite ora su ESP-NOW)

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

    bool isConnected();
    void pushState();

    // Collega il modulo social (ESP-NOW) così la companion app può leggere i vicini.
    void attachSocial(MochiNow* now);

    const String& getBleName() const { return bleName; }
};

#endif // MOCHI_BLE_H
