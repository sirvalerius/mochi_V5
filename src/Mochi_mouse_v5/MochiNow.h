#ifndef MOCHI_NOW_H
#define MOCHI_NOW_H

#include <Arduino.h>
#include <esp_now.h>
#include "MochiState.h"

// Tipi di pacchetto scambiati tra Mochi via ESP-NOW.
enum MochiPktType : uint8_t {
    PKT_ANNOUNCE     = 0, // Broadcast periodico di presenza
    PKT_VISIT        = 1, // Consegna dell'avatar in visita (unicast)
    PKT_VISIT_ACK    = 2, // Risposta all'host (unicast): accettato o occupato
    PKT_FRIEND_REQ   = 3, // Richiesta di amicizia (unicast)
    PKT_FRIEND_ACCEPT= 4  // Accettazione di una richiesta (unicast) → amicizia mutua
};

// Pacchetto ESP-NOW (max 250 byte: questo è ~226).
typedef struct {
    uint8_t type;        // MochiPktType
    uint8_t ackOk;       // Per PKT_VISIT_ACK: 1 = accettato, 0 = occupato
    char    id[24];      // ID del mittente (es. "MOCHI-ABCDEF")
    char    payload[200];// Per PKT_VISIT: avatar JSON; altrimenti vuoto
} MochiPacket;

// Un Mochi vicino rilevato via ESP-NOW.
struct NearbyMochi {
    String        id;        // ID stabile (es. "MOCHI-ABCDEF")
    uint8_t       mac[6];    // MAC del peer (per inviargli pacchetti)
    int           rssi;      // Potenza segnale
    unsigned long lastSeen;  // millis() dell'ultimo annuncio ricevuto
};

class MochiNow {
private:
    MochiState* mochi;
    String      selfId;
    uint8_t     selfMac[6];
    bool        ready = false;

    NearbyMochi   nearby[MAX_NEARBY];
    int           nearbyLen = 0;

    unsigned long lastAnnounce = 0;
    unsigned long lastVisitCheck = 0;
    unsigned long lastVisitDepart = 0;

    // Partenza in attesa di ack (la visita parte solo su ack positivo)
    bool          awaitingAck = false;
    uint8_t       pendingMac[6];
    String        pendingHostId = "";
    unsigned long pendingSentAt = 0;

    // Accettazione amicizia ricevuta via ESP-NOW: l'addFriend (scrive in NVS)
    // viene differito al tick() del loop principale per non bloccare la callback.
    String        inboundAcceptId = "";

    // --- DIAGNOSTICA ---
    unsigned long announceCount = 0; // Annunci broadcast inviati
    unsigned long recvCount = 0;     // Pacchetti ESP-NOW ricevuti
    int           lastSendStatus = -1; // -1=mai, 0=successo, 1=fallito
    String        lastRecvId = "";   // ID dell'ultimo mittente

    void computeSelfId();
    void sendAnnounce();
    void sendVisit(NearbyMochi& target);
    void sendAck(const uint8_t* mac, bool ok);
    int  findNearbyIndex(const String& id);
    bool sendFriendPkt(const String& id, uint8_t type);
    void ensurePeer(const uint8_t* mac);
    void reportNearby(const String& id, const uint8_t* mac, int rssi);
    void pruneNearby(unsigned long now);
    void tickVisit(unsigned long now);

public:
    MochiNow(MochiState* m);
    bool begin();
    void tick(unsigned long now);            // Annuncio + prune + logica visite
    void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    void onSend(int status);                 // Esito invio (dalla send callback)

    // Invia una richiesta/accettazione di amicizia a un Mochi vicino (per id).
    bool   sendFriendRequest(const String& id);
    bool   sendFriendAccept(const String& id);

    // DEBUG (temporaneo): forza subito una visita verso il Mochi vicino indicato,
    // bypassando cooldown e probabilità. Ritorna false se non è nei paraggi/occupato.
    bool   forceVisit(const String& id);

    int    nearbyCount() const { return nearbyLen; }
    String getNearbyJson();
    String getDebugReport();                 // Report diagnostico per la companion app
    const String& getSelfId() const { return selfId; }
};

#endif // MOCHI_NOW_H
