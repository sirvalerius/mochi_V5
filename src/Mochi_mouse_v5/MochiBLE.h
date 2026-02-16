#ifndef MOCHI_BLE_H
#define MOCHI_BLE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>
#include "MochiState.h"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// 1. Aggiungi questa classe SOPRA la classe MochiBLE
class MochiServerCallbacks: public BLEServerCallbacks {
    Adafruit_NeoPixel* led;
public:
    MochiServerCallbacks(Adafruit_NeoPixel* l) : led(l) {}

    void onConnect(BLEServer* pServer) {
        if(led) {
            led->setPixelColor(0, led->Color(0, 255, 140));
            led->show();
        }
        Serial.println("BLE: Device Connesso");
    }

    void onDisconnect(BLEServer* pServer) {
        if(led) {
            led->setPixelColor(0, led->Color(0, 0, 0)); // Spento
            led->show();
        }
        // Molto importante: riavvia l'advertising o non potrai riconnetterti!
        pServer->getAdvertising()->start();
        Serial.println("BLE: Device Disconnesso - Advertising riavviato");
    }
};

class MochiBLECallbacks : public BLECharacteristicCallbacks {
    MochiState* statePtr;
public:
    MochiBLECallbacks(MochiState* s) : statePtr(s) {}

    void onWrite(BLECharacteristic *pCharacteristic) {
        String cmd = pCharacteristic->getValue(); 

        if (cmd.length() > 0) {
            Serial.println("Ricevuto BLE: " + cmd);
            
            if (cmd.startsWith("unix:")) {
                // Estraiamo il timestamp saltando i primi 5 caratteri ("unix:")
                long timestamp = atol(cmd.substring(5).c_str());
                statePtr->syncTime(timestamp);
            } else if (cmd.startsWith("set_json:")) {
                // Estrae tutto quello che c'è dopo "set_json:" e lo salva così com'è
                String json = cmd.substring(9); 
                statePtr->saveSettings(json);
                Serial.println("Settings salvati.");
            } 
            else if (cmd == "get_json") {
                // Prende la stringa salvata e la rimanda al browser
                pCharacteristic->setValue(statePtr->settingsBlob.c_str());
                pCharacteristic->notify();
                Serial.println("Settings inviati al browser.");
            } else {
                // Gestione dei comandi normali (FEED, PLAY, ecc.)
                statePtr->applyCommand(cmd);
            }
        }
    }
};

class MochiBLE {
private:
    BLEServer* pServer = NULL;
    BLECharacteristic* pCharacteristic = NULL;
    Adafruit_NeoPixel* ledPtr;
    MochiState* statePtr;

public:
    MochiBLE(MochiState* s, Adafruit_NeoPixel* l) : statePtr(s), ledPtr(l) {}

    void begin() {
        uint32_t chipId = 0;
        for(int i=0; i<17; i=i+8) { chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i; }
        String deviceName = "Mochi-" + String(chipId, HEX);
        deviceName.toUpperCase();

        BLEDevice::init(deviceName.c_str());
        pServer = BLEDevice::createServer();

        pServer->setCallbacks(new MochiServerCallbacks(ledPtr));
        
        BLEService *pService = pServer->createService(SERVICE_UUID);
        pCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID,
                            BLECharacteristic::PROPERTY_READ   |
                            BLECharacteristic::PROPERTY_WRITE  |
                            BLECharacteristic::PROPERTY_NOTIFY
                          );

        pCharacteristic->setCallbacks(new MochiBLECallbacks(statePtr));
        pCharacteristic->addDescriptor(new BLE2902());

        pService->start();

        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        
        // Questo assicura che il nome e l'UUID siano visibili subito durante lo scan
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);  
        pAdvertising->setMinPreferred(0x12);
        
        // Avvia l'advertising usando l'oggetto pAdvertising
        pAdvertising->start(); 
        
        Serial.println("Advertising avviato correttamente!");
        Serial.println("BLE Pronto come: " + deviceName);
    }

    void scanForFriends() {
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->setActiveScan(true);
        
        // FIX 2: start() restituisce un puntatore BLEScanResults*
        BLEScanResults* foundDevices = pBLEScan->start(2, false);
        
        for (int i = 0; i < foundDevices->getCount(); i++) {
            BLEAdvertisedDevice device = foundDevices->getDevice(i);
            String name = String(device.getName().c_str());
            if (name.startsWith("Mochi-")) {
                Serial.print("Amico trovato: ");
                Serial.println(name);
                statePtr->applyCommand("friend_nearby");
            }
        }
        pBLEScan->clearResults();
    }

    bool isConnected() {
        return pServer->getConnectedCount() > 0;
    }
};

#endif