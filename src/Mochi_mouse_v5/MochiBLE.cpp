#include "MochiBLE.h"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ================================================================
// CLASSI CALLBACK
// ================================================================

class MyServerCallbacks: public BLEServerCallbacks {
    Adafruit_NeoPixel* led;
public:
    MyServerCallbacks(Adafruit_NeoPixel* l) : led(l) {}

    void onConnect(BLEServer* pServer) {
        if(led) {
            led->setPixelColor(0, led->Color(0, 255, 140)); // Colore del tuo vecchio sketch
            led->show();
        }
        Serial.println("BLE: Device Connesso");
    }

    void onDisconnect(BLEServer* pServer) {
        if(led) {
            led->setPixelColor(0, led->Color(0, 0, 0)); // Spento
            led->show();
        }
        pServer->getAdvertising()->start();
        Serial.println("BLE: Device Disconnesso - Advertising riavviato");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    MochiState* statePtr;
public:
    MyCallbacks(MochiState* s) : statePtr(s) {}

    void onWrite(BLECharacteristic *pCharacteristic) {
        String cmd = pCharacteristic->getValue(); 

        if (cmd.length() > 0) {
            Serial.println("Ricevuto BLE: " + cmd);
            
            if (cmd.startsWith("unix:")) {
                long timestamp = atol(cmd.substring(5).c_str());
                statePtr->syncTime(timestamp);
            } else if (cmd.startsWith("set_json:")) {
                String json = cmd.substring(9); 
                statePtr->saveSettings(json);
                statePtr->saveState(); 
                Serial.println("Settings salvati.");
            } else if (cmd == "get_json") {
                pCharacteristic->setValue(statePtr->settingsBlob.c_str());
                pCharacteristic->notify();
                Serial.println("Settings inviati al browser.");
            } else {
                statePtr->applyCommand(cmd);
            }
        }
    }
};

// ================================================================
// IMPLEMENTAZIONE CLASSE
// ================================================================

MochiBLE::MochiBLE(MochiState* m, Adafruit_NeoPixel* led) {
    mochi = m;
    statusLed = led;
    pServer = nullptr;
    pCharacteristic = nullptr;

    uint32_t chipId = 0;
    for(int i=0; i<17; i=i+8) { chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i; }
    String deviceName = "Mochi-" + String(chipId, HEX);
    deviceName.toUpperCase();
    bleName = deviceName;
}

void MochiBLE::begin() {
    Serial.println("Inizializzazione BLE...");
    BLEDevice::init(bleName.c_str());
    BLEDevice::setMTU(512); // FONDAMENTALE PER I JSON LUNGHI!

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks(statusLed));

    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Singola caratteristica tuttofare (come nel tuo codice originale)
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ   |
                        BLECharacteristic::PROPERTY_WRITE  |
                        BLECharacteristic::PROPERTY_WRITE_NR | // Aggiunto NR per Chrome web-bluetooth
                        BLECharacteristic::PROPERTY_NOTIFY
                      );

    pCharacteristic->setCallbacks(new MyCallbacks(mochi));
    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  
    pAdvertising->setMinPreferred(0x12);
    
    pAdvertising->start(); 
    Serial.println("BLE Pronto come: " + bleName);
}

void MochiBLE::scanForFriends() {
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    
    BLEScanResults* foundDevices = pBLEScan->start(2, false);
    
    for (int i = 0; i < foundDevices->getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices->getDevice(i);
        String name = String(device.getName().c_str());
        if (name.startsWith("Mochi-")) {
            Serial.print("Amico trovato: ");
            Serial.println(name);
            mochi->applyCommand("friend_nearby");
        }
    }
    pBLEScan->clearResults();
}

bool MochiBLE::isConnected() {
    if (pServer != nullptr) {
        return pServer->getConnectedCount() > 0;
    }
    return false;
}