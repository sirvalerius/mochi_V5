#ifndef MOCHI_BLE_H
#define MOCHI_BLE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>
#include "MochiState.h"

// Un Mochi vicino rilevato dallo scan BLE.
struct NearbyMochi {
    String        id;        // ID stabile (es. "Mochi-ABCD")
    BLEAddress    addr;      // Indirizzo BLE (per le future connessioni in visita)
    int           rssi;      // Potenza segnale (negativo, ~ -40 vicino, -90 lontano)
    unsigned long lastSeen;  // millis() dell'ultimo rilevamento

    NearbyMochi() : addr((uint8_t*)"\0\0\0\0\0\0"), rssi(0), lastSeen(0) {}
};

class MochiBLE {
private:
    MochiState* mochi;
    Adafruit_NeoPixel* statusLed;
    String bleName;
    BLEServer* pServer;
    BLECharacteristic* pCharacteristic; // Ripristinato il puntatore

    // --- DISCOVERY (scan async) ---
    BLEScan*      pBLEScan = nullptr;
    bool          scanning = false;        // Scan in corso?
    unsigned long lastScanStart = 0;       // millis() dell'ultimo avvio scan
    NearbyMochi   nearby[MAX_NEARBY];
    int           nearbyLen = 0;

    // --- VISITE (ruolo central) ---
    BLEClient*    pClient = nullptr;       // Riusato per ogni handover (no leak)
    unsigned long lastVisitCheck = 0;      // Ultima valutazione di partenza
    unsigned long lastVisitDepart = 0;     // Ultima partenza riuscita (per il cooldown)
    bool          attemptVisit(NearbyMochi& target); // Connette e consegna l'ospite

public:
    MochiBLE(MochiState* m, Adafruit_NeoPixel* led);
    void begin();

    bool isConnected();
    void pushState();

    // --- DISCOVERY ---
    void tickScan(unsigned long now);          // Avvia uno scan async se è ora
    void onScanComplete();                     // Chiamata al termine dello scan
    void reportNearby(BLEAdvertisedDevice dev); // Upsert da una advertised device
    void pruneNearby(unsigned long now);        // Rimuove i vicini scaduti
    int  nearbyCount() const { return nearbyLen; }
    String getNearbyJson();                     // Lista vicini per la companion app

    // --- VISITE ---
    void tickVisit(unsigned long now);          // Valuta e avvia eventualmente una visita

    const String& getBleName() const { return bleName; }
};

#endif // MOCHI_BLE_H