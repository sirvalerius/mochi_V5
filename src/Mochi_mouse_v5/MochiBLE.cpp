#include "MochiBLE.h"
#include "MochiNow.h"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Modulo social (ESP-NOW): la companion app legge i vicini tramite questo.
static MochiNow* g_social = nullptr;

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
                // Sempre con i colori reali del Mochi, così la pagina adotta lo
                // schema colore del device appena connesso.
                String reply = statePtr->getSettingsJson();
                pCharacteristic->setValue(reply.c_str());
                pCharacteristic->notify();
                Serial.println("Settings inviati al browser.");
            } else if (cmd == "get_settings") {
                String reply = statePtr->settingsBlob;
                pCharacteristic->setValue(reply.c_str());
                pCharacteristic->notify();
                Serial.println("[BLE] Impostazioni inviate al browser!");
                return;
            } else if (cmd == "get_state") {
                String reply = statePtr->getStateJson();
                pCharacteristic->setValue(reply.c_str());
                pCharacteristic->notify();
                Serial.println("[BLE] Stato inviato al browser!");
                return;
            } else if (cmd == "get_nearby") {
                // I vicini ora arrivano da ESP-NOW (modulo MochiNow).
                String reply = g_social ? g_social->getNearbyJson() : "[]";
                pCharacteristic->setValue(reply.c_str());
                pCharacteristic->notify();
                Serial.println("[BLE] Lista vicini inviata al browser!");
                return;
            } else if (cmd == "get_friends") {
                String reply = statePtr->getFriendsJson();
                pCharacteristic->setValue(reply.c_str());
                pCharacteristic->notify();
                Serial.println("[BLE] Lista amici inviata al browser!");
                return;
            } else if (cmd == "get_requests") {
                String reply = statePtr->getRequestsJson();
                pCharacteristic->setValue(reply.c_str());
                pCharacteristic->notify();
                Serial.println("[BLE] Richieste di amicizia inviate al browser!");
                return;
            } else if (cmd.startsWith("add_friend:")) {
                // Non aggiunge subito: invia una RICHIESTA di amicizia via ESP-NOW.
                // L'amicizia nasce solo quando l'altro Mochi accetta.
                if (g_social) g_social->sendFriendRequest(cmd.substring(11));
                return;
            } else if (cmd.startsWith("accept_friend:")) {
                // Accetta una richiesta in arrivo: diventiamo amici e avvisiamo
                // l'altro Mochi (che a sua volta ci aggiungerà → amicizia mutua).
                String id = cmd.substring(14);
                statePtr->addFriend(id);
                statePtr->removePendingRequest(id);
                if (g_social) g_social->sendFriendAccept(id);
                return;
            } else if (cmd.startsWith("decline_friend:")) {
                statePtr->removePendingRequest(cmd.substring(15));
                return;
            } else if (cmd.startsWith("del_friend:")) {
                statePtr->removeFriend(cmd.substring(11));
                return;
            } else if (cmd.startsWith("force_visit:")) {
                // DEBUG (temporaneo): forza la partenza in visita verso l'amico.
                if (g_social) g_social->forceVisit(cmd.substring(12));
                return;
            } else if (cmd == "get_debug") {
                String reply = g_social ? g_social->getDebugReport() : "DBG\nsocial non collegato";
                pCharacteristic->setValue(reply.c_str());
                pCharacteristic->notify();
                Serial.println("[BLE] Report debug inviato al browser!");
                return;
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

void MochiBLE::attachSocial(MochiNow* now) {
    g_social = now;
}

bool MochiBLE::isConnected() {
    if (pServer != nullptr) {
        return pServer->getConnectedCount() > 0;
    }
    return false;
}

void MochiBLE::pushState() {
    if (!isConnected() || !pCharacteristic) return;
    String state = mochi->getStateJson();
    pCharacteristic->setValue(state.c_str());
    pCharacteristic->notify();
}
