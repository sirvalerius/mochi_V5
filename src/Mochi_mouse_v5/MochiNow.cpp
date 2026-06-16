#include "MochiNow.h"
#include <WiFi.h>
#include <esp_wifi.h>

// Ponte verso l'istanza per la callback C di ricezione ESP-NOW.
static MochiNow* g_now = nullptr;

static const uint8_t BCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static bool macEqual(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, 6) == 0;
}

static void onRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (g_now) g_now->onRecv(info, data, len);
}

MochiNow::MochiNow(MochiState* m) {
    mochi = m;
    g_now = this;
    memset(selfMac, 0, 6);
    memset(pendingMac, 0, 6);
}

// Stesso schema di MochiBLE: ID stabile derivato dal MAC efuse, per restare
// coerente con gli amici già salvati (es. "MOCHI-ABCDEF").
void MochiNow::computeSelfId() {
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i = i + 8) { chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i; }
    String name = "Mochi-" + String(chipId, HEX);
    name.toUpperCase();
    selfId = name;
}

bool MochiNow::begin() {
    computeSelfId();

    // ESP-NOW richiede il WiFi in STA, senza connettersi ad alcun access point.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    WiFi.macAddress(selfMac);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[NOW] esp_now_init FALLITO!");
        return false;
    }
    esp_now_register_recv_cb(onRecvStatic);

    // Peer broadcast per gli annunci di presenza.
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BCAST_MAC, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    ready = true;
    Serial.println("[NOW] ESP-NOW pronto come: " + selfId);
    return true;
}

void MochiNow::ensurePeer(const uint8_t* mac) {
    if (esp_now_is_peer_exist(mac)) return;
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void MochiNow::sendAnnounce() {
    MochiPacket pkt = {};
    pkt.type = PKT_ANNOUNCE;
    strncpy(pkt.id, selfId.c_str(), sizeof(pkt.id) - 1);
    esp_now_send(BCAST_MAC, (const uint8_t*)&pkt, sizeof(pkt));
}

void MochiNow::sendVisit(NearbyMochi& target) {
    MochiPacket pkt = {};
    pkt.type = PKT_VISIT;
    strncpy(pkt.id, selfId.c_str(), sizeof(pkt.id) - 1);
    String payload = mochi->getVisitPayloadJson(selfId);
    strncpy(pkt.payload, payload.c_str(), sizeof(pkt.payload) - 1);

    ensurePeer(target.mac);
    esp_now_send(target.mac, (const uint8_t*)&pkt, sizeof(pkt));

    // In attesa dell'ack: la partenza diventa effettiva solo se l'host accetta.
    awaitingAck = true;
    memcpy(pendingMac, target.mac, 6);
    pendingHostId = target.id;
    pendingSentAt = millis();
    Serial.println("[NOW] Richiesta di visita inviata a " + target.id);
}

void MochiNow::sendAck(const uint8_t* mac, bool ok) {
    MochiPacket pkt = {};
    pkt.type  = PKT_VISIT_ACK;
    pkt.ackOk = ok ? 1 : 0;
    strncpy(pkt.id, selfId.c_str(), sizeof(pkt.id) - 1);
    ensurePeer(mac);
    esp_now_send(mac, (const uint8_t*)&pkt, sizeof(pkt));
}

// Upsert di un vicino dalla ricezione di un annuncio.
void MochiNow::reportNearby(const String& id, const uint8_t* mac, int rssi) {
    unsigned long now = millis();
    for (int i = 0; i < nearbyLen; i++) {
        if (nearby[i].id == id) {
            memcpy(nearby[i].mac, mac, 6);
            nearby[i].rssi = rssi;
            nearby[i].lastSeen = now;
            return;
        }
    }
    if (nearbyLen < MAX_NEARBY) {
        nearby[nearbyLen].id = id;
        memcpy(nearby[nearbyLen].mac, mac, 6);
        nearby[nearbyLen].rssi = rssi;
        nearby[nearbyLen].lastSeen = now;
        nearbyLen++;
        ensurePeer(mac); // così potremo poi inviargli una visita
        Serial.println("[NOW] Mochi vicino: " + id + " (rssi " + String(rssi) + ")");
    }
}

void MochiNow::pruneNearby(unsigned long now) {
    int w = 0;
    for (int i = 0; i < nearbyLen; i++) {
        if (now - nearby[i].lastSeen <= NEARBY_TIMEOUT_MS) {
            if (w != i) nearby[w] = nearby[i];
            w++;
        } else {
            esp_now_del_peer(nearby[i].mac); // libera il peer scaduto
        }
    }
    nearbyLen = w;
}

void MochiNow::onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len < (int)sizeof(uint8_t)) return;
    MochiPacket pkt = {};
    memcpy(&pkt, data, min(len, (int)sizeof(pkt)));
    pkt.id[sizeof(pkt.id) - 1] = '\0';
    pkt.payload[sizeof(pkt.payload) - 1] = '\0';

    String senderId = String(pkt.id);
    if (senderId.length() == 0 || senderId == selfId) return;

    int rssi = (info->rx_ctrl) ? info->rx_ctrl->rssi : 0;

    switch (pkt.type) {
        case PKT_ANNOUNCE:
            reportNearby(senderId, info->src_addr, rssi);
            break;

        case PKT_VISIT: {
            // Un altro Mochi ci consegna il suo avatar: proviamo ad ospitarlo.
            bool ok = mochi->receiveGuest(String(pkt.payload), VISIT_DURATION_MS);
            sendAck(info->src_addr, ok);
            break;
        }

        case PKT_VISIT_ACK:
            if (awaitingAck && macEqual(info->src_addr, pendingMac)) {
                if (pkt.ackOk) {
                    mochi->goAway(pendingHostId, VISIT_DURATION_MS);
                    lastVisitDepart = millis();
                    Serial.println("[NOW] Visita accettata, parto!");
                } else {
                    Serial.println("[NOW] Host occupato, resto a casa.");
                }
                awaitingAck = false;
            }
            break;
    }
}

// Valuta se far partire il Mochi in visita da un amico vicino.
void MochiNow::tickVisit(unsigned long now) {
    if (mochi->isAway || mochi->isHostingGuest) return;

    if (awaitingAck) {
        if (now - pendingSentAt > VISIT_ACK_TIMEOUT_MS) {
            awaitingAck = false; // Nessun ack: rinuncia, resta a casa
            Serial.println("[NOW] Nessun ack, visita annullata.");
        }
        return;
    }

    if (lastVisitCheck != 0 && (now - lastVisitCheck) < VISIT_CHECK_MS) return;
    lastVisitCheck = now;

    if (lastVisitDepart != 0 && (now - lastVisitDepart) < VISIT_COOLDOWN_MS) return;

    int best = -1;
    for (int i = 0; i < nearbyLen; i++) {
        if (nearby[i].rssi >= VISIT_RSSI_MIN && mochi->isFriend(nearby[i].id)) {
            if (best < 0 || nearby[i].rssi > nearby[best].rssi) best = i;
        }
    }
    if (best < 0) return;

    if (random(1000) >= VISIT_CHANCE) return;

    sendVisit(nearby[best]);
}

void MochiNow::tick(unsigned long now) {
    if (!ready) return;
    pruneNearby(now);
    if (lastAnnounce == 0 || (now - lastAnnounce) >= ANNOUNCE_INTERVAL_MS) {
        sendAnnounce();
        lastAnnounce = now;
    }
    tickVisit(now);
}

String MochiNow::getNearbyJson() {
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
