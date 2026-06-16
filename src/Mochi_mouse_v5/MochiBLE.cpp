#include "MochiBLE.h"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Le callback di scan BLE sono statiche/globali: serve un ponte verso l'istanza.
static MochiBLE* g_self = nullptr;

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

// Riceve i device trovati durante lo scan async.
class MochiScanCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) {
        if (g_self) g_self->reportNearby(dev);
    }
};

// Chiamata da BLEScan al termine dello scan async.
static void scanCompleteCB(BLEScanResults results) {
    if (g_self) g_self->onScanComplete();
}

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
                String reply = g_self ? g_self->getNearbyJson() : "[]";
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
            } else if (cmd.startsWith("add_friend:")) {
                statePtr->addFriend(cmd.substring(11));
                return;
            } else if (cmd.startsWith("del_friend:")) {
                statePtr->removeFriend(cmd.substring(11));
                return;
            } else if (cmd.startsWith("visit:")) {
                // Un altro Mochi ci sta consegnando il suo avatar come ospite.
                bool ok = statePtr->receiveGuest(cmd.substring(6), VISIT_DURATION_MS);
                // L'ack viene letto dal Mochi visitatore sulla stessa connessione.
                pCharacteristic->setValue(ok ? "visit_ack:OK" : "visit_busy");
                Serial.println(ok ? "[BLE] Ospite accettato!" : "[BLE] Visita rifiutata (occupato)");
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
    g_self = this; // Ponte per le callback statiche di scan

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

    // --- DISCOVERY: scan async non bloccante ---
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MochiScanCallbacks(), false);
    pBLEScan->setActiveScan(true); // Active: chiede anche il nome (scan response)
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

// Avvia uno scan async se è passato SCAN_INTERVAL_MS e non ce n'è uno in corso.
void MochiBLE::tickScan(unsigned long now) {
    pruneNearby(now);

    if (scanning || pBLEScan == nullptr) return;
    if (lastScanStart != 0 && (now - lastScanStart) < SCAN_INTERVAL_MS) return;

    scanning = true;
    lastScanStart = now;
    // start() async: ritorna subito, scanCompleteCB chiamata alla fine.
    pBLEScan->start(SCAN_DURATION_S, scanCompleteCB, false);
}

void MochiBLE::onScanComplete() {
    scanning = false;
    pruneNearby(millis());
    if (pBLEScan) pBLEScan->clearResults(); // Libera la RAM dei risultati grezzi
}

// Upsert di un device trovato nella tabella dei vicini.
void MochiBLE::reportNearby(BLEAdvertisedDevice dev) {
    String name = String(dev.getName().c_str());
    if (!name.startsWith("Mochi-")) return; // Solo altri Mochi
    if (name == bleName) return;            // Mai me stesso

    unsigned long now = millis();
    int rssi = dev.getRSSI();

    // Cerca un'entry esistente con lo stesso id e aggiornala.
    for (int i = 0; i < nearbyLen; i++) {
        if (nearby[i].id == name) {
            nearby[i].addr = dev.getAddress();
            nearby[i].rssi = rssi;
            nearby[i].lastSeen = now;
            return;
        }
    }

    // Nuovo vicino: aggiungi se c'è spazio.
    if (nearbyLen < MAX_NEARBY) {
        nearby[nearbyLen].id = name;
        nearby[nearbyLen].addr = dev.getAddress();
        nearby[nearbyLen].rssi = rssi;
        nearby[nearbyLen].lastSeen = now;
        nearbyLen++;
        Serial.println("[BLE] Mochi vicino: " + name + " (rssi " + String(rssi) + ")");
    }
}

// Rimuove i vicini non più visti da NEARBY_TIMEOUT_MS (compattando l'array).
void MochiBLE::pruneNearby(unsigned long now) {
    int w = 0;
    for (int i = 0; i < nearbyLen; i++) {
        if (now - nearby[i].lastSeen <= NEARBY_TIMEOUT_MS) {
            if (w != i) nearby[w] = nearby[i];
            w++;
        }
    }
    nearbyLen = w;
}

// Valuta periodicamente se far partire il Mochi in visita da un amico vicino.
void MochiBLE::tickVisit(unsigned long now) {
    if (mochi == nullptr) return;
    if (mochi->isAway || mochi->isHostingGuest) return; // Serializzazione: una cosa alla volta
    if (lastVisitCheck != 0 && (now - lastVisitCheck) < VISIT_CHECK_MS) return;
    lastVisitCheck = now;

    // Cooldown tra una partenza e l'altra
    if (lastVisitDepart != 0 && (now - lastVisitDepart) < VISIT_COOLDOWN_MS) return;

    // Cerca l'amico vicino col segnale più forte sopra la soglia
    int best = -1;
    for (int i = 0; i < nearbyLen; i++) {
        if (nearby[i].rssi >= VISIT_RSSI_MIN && mochi->isFriend(nearby[i].id)) {
            if (best < 0 || nearby[i].rssi > nearby[best].rssi) best = i;
        }
    }
    if (best < 0) return; // Nessun amico in range

    // Tiro di probabilità (VISIT_CHANCE su 1000)
    if (random(1000) >= VISIT_CHANCE) return;

    attemptVisit(nearby[best]);
}

// Ruolo CENTRAL: connette all'host, consegna l'avatar, attende l'ack.
bool MochiBLE::attemptVisit(NearbyMochi& target) {
    if (mochi == nullptr) return false;

    // Evita conflitti: ferma lo scan async prima di connettere come central.
    if (scanning && pBLEScan) { pBLEScan->stop(); scanning = false; }

    if (pClient == nullptr) pClient = BLEDevice::createClient();

    Serial.println("[BLE] Tentativo di visita verso " + target.id);
    // Timeout finito: senza, connect() bloccherebbe all'infinito se il peer sparisce.
    if (!pClient->connect(target.addr, 0xFF, 4000)) {
        Serial.println("[BLE] Connessione all'amico fallita.");
        if (pClient->isConnected()) pClient->disconnect();
        return false;
    }

    bool sent = false;
    BLERemoteService* svc = pClient->getService(SERVICE_UUID);
    if (svc) {
        BLERemoteCharacteristic* ch = svc->getCharacteristic(CHARACTERISTIC_UUID);
        if (ch && ch->canWrite()) {
            String payload = "visit:" + mochi->getVisitPayloadJson(bleName);
            ch->writeValue((uint8_t*)payload.c_str(), payload.length(), true); // con response
            // Leggi l'ack sulla stessa connessione: la partenza è atomica.
            String ack = ch->canRead() ? String(ch->readValue().c_str()) : "";
            if (ack.startsWith("visit_ack:OK")) {
                mochi->goAway(target.id, VISIT_DURATION_MS);
                sent = true;
                Serial.println("[BLE] Visita accettata, parto!");
            } else {
                Serial.println("[BLE] Ack negativo (\"" + ack + "\"), resto a casa.");
            }
        }
    }
    pClient->disconnect();
    if (sent) lastVisitDepart = millis();
    return sent;
}

// Lista vicini in JSON per la companion app (con flag isFriend).
String MochiBLE::getNearbyJson() {
    String out = "[";
    for (int i = 0; i < nearbyLen; i++) {
        if (i > 0) out += ",";
        bool fr = mochi && mochi->isFriend(nearby[i].id);
        out += "{\"id\":\"" + nearby[i].id + "\",\"rssi\":" + String(nearby[i].rssi) +
               ",\"isFriend\":" + (fr ? "true" : "false") + "}";
    }
    out += "]";
    return out;
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