#include "MochiBLE.h"

// --- CONFIGURAZIONE UUID ---
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "12345678-1234-5678-1234-56789abcdef0" 

// Gestore Connessione/Disconnessione
class MyServerCallbacks: public BLEServerCallbacks {
    Adafruit_NeoPixel* led;
public:
    // Il costruttore riceve il LED per poterlo cambiare
    MyServerCallbacks(Adafruit_NeoPixel* l) : led(l) {}

    void onConnect(BLEServer* pServer) {
        if(led) {
            led->setPixelColor(0, led->Color(0, 0, 255)); // Blu = Bluetooth connesso
            led->show();
        }
    }

    void onDisconnect(BLEServer* pServer) {
        if(led) {
            led->setPixelColor(0, led->Color(0, 255, 0)); // Verde = Disconnesso/Pronto
            led->show();
        }
        // Riavvia l'advertising per permettere di ricollegarsi
        pServer->startAdvertising(); 
    }
};

// Gestore Ricezione Dati
class MyCallbacks: public BLECharacteristicCallbacks {
    MochiState* mochi;
public:
    // Il costruttore riceve lo stato del Mochi per inviargli comandi
    MyCallbacks(MochiState* m) : mochi(m) {}

    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        
        if (rxValue.length() > 0) {
            String command = "";
            for (int i = 0; i < rxValue.length(); i++) {
                command += rxValue[i];
            }
            
            // Passa il comando allo stato centrale
            if (mochi) {
                mochi->lastCommand = command;
            }
            Serial.print("Ricevuto via BLE: ");
            Serial.println(command);
        }
    }
};

MochiBLE::MochiBLE(MochiState* m, Adafruit_NeoPixel* led, const char* name) {
    mochi = m;
    statusLed = led;
    bleName = name;
    pServer = nullptr;
}

void MochiBLE::begin() {
    Serial.println("Inizializzazione BLE...");
    BLEDevice::init(bleName);

    // Crea il Server e assegna le Callback passandogli il LED
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks(statusLed));

    // Crea il Servizio
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Caratteristica TX (Notifiche dal Mochi al Telefono, opzionale)
    BLECharacteristic *pTxCharacteristic = pService->createCharacteristic(
                                        CHARACTERISTIC_UUID_TX,
                                        BLECharacteristic::PROPERTY_NOTIFY
                                    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // Caratteristica RX (Scrittura dal Telefono al Mochi)
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                                        CHARACTERISTIC_UUID_RX,
                                        BLECharacteristic::PROPERTY_WRITE
                                    );
    // Assegna la callback passandogli lo stato del Mochi
    pRxCharacteristic->setCallbacks(new MyCallbacks(mochi));

    // Avvia tutto
    pService->start();
    pServer->getAdvertising()->start();
    
    Serial.println("BLE in ascolto!");
}