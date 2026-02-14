#ifndef MOCHI_BLE_H
#define MOCHI_BLE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "MochiState.h"

// UUID Univoci (puoi generarne di nuovi, ma questi sono pronti all'uso)
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MochiBLECallbacks : public BLECharacteristicCallbacks {
    MochiState* statePtr;
public:
    MochiBLECallbacks(MochiState* s) : statePtr(s) {}

    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            String command = String(value.c_str());
            Serial.print("BLE Ricevuto: ");
            Serial.println(command);
            statePtr->applyCommand(command);
        }
    }
};

class MochiBLE {
private:
    BLEServer* pServer = NULL;
    BLECharacteristic* pCharacteristic = NULL;
    MochiState* statePtr;
    bool deviceConnected = false;

public:
    MochiBLE(MochiState* s) : statePtr(s) {}

    void begin() {
      // Recupera l'ID univoco del chip
        uint32_t chipId = 0;
        for(int i=0; i<17; i=i+8) { chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i; }
        String deviceName = "Mochi-" + String(chipId, HEX);
        deviceName.toUpperCase();

        BLEDevice::init(deviceName.c_str());

        // Crea il Server BLE
        pServer = BLEDevice::createServer();
        
        // Crea il Servizio
        BLEService *pService = pServer->createService(SERVICE_UUID);

        // Crea la Caratteristica per ricevere i comandi (WRITE)
        pCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID,
                            BLECharacteristic::PROPERTY_READ   |
                            BLECharacteristic::PROPERTY_WRITE  |
                            BLECharacteristic::PROPERTY_NOTIFY
                          );

        // Collega i callback per gestire l'arrivo dei dati
        pCharacteristic->setCallbacks(new MochiBLECallbacks(statePtr));
        pCharacteristic->addDescriptor(new BLE2902());

        // Avvia il servizio
        pService->start();

        // Avvia l'Advertising (fatti trovare!)
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);  
        pAdvertising->setMinPreferred(0x12);
        BLEDevice::startAdvertising();
        Serial.println("Mochi pronto come: " + deviceName);
        Serial.println("BLE in attesa di connessione...");
    }

    bool isConnected() {
        return pServer->getConnectedCount() > 0;
    }

    void scanForFriends() {
      BLEScan* pBLEScan = BLEDevice::getScan();
      pBLEScan->setActiveScan(true);
      pBLEScan->setInterval(100);
      pBLEScan->setWindow(99);
      
      // Cerca per 2 secondi
      BLEScanResults foundDevices = pBLEScan->start(2, false);
      for (int i = 0; i < foundDevices.getCount(); i++) {
          BLEAdvertisedDevice device = foundDevices.getDevice(i);
          if (device.getName().startsWith("Mochi-")) {
              Serial.print("Ho trovato un amico: ");
              Serial.println(device.getName().c_str());
              // Qui potrai scatenare un'animazione di "saluto"
              statePtr->applyCommand("friend_nearby");
          }
      }
      pBLEScan->clearResults();
  }
};

#endif