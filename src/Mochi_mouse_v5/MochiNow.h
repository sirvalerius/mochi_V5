#ifndef MOCHI_NOW_H
#define MOCHI_NOW_H

#include <Arduino.h>
#include <esp_now.h>
#include "MochiState.h"

// Tipi di pacchetto scambiati tra Mochi via ESP-NOW.
enum MochiPktType : uint8_t {
    PKT_ANNOUNCE  = 0, // Broadcast periodico di presenza
    PKT_VISIT     = 1, // Consegna dell'avatar in visita (unicast)
    PKT_VISIT_ACK = 2  // Risposta all'host (unicast): accettato o occupato
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

    // --- DIAGNOSTICA ---
    unsigned long announceCount = 0; // Annunci broadcast inviati
    unsigned long recvCount = 0;     // Pacchetti ESP-NOW ricevuti
    int           lastSendStatus = -1; // -1=mai, 0=successo, 1=fallito
    String        lastRecvId = "";   // ID dell'ultimo mittente

    void computeSelfId();
    void sendAnnounce();
    void sendVisit(NearbyMochi& target);
    void sendAck(const uint8_t* mac, bool ok);
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

    int    nearbyCount() const { return nearbyLen; }
    String getNearbyJson();
    String getDebugReport();                 // Report diagnostico per la companion app
    const String& getSelfId() const { return selfId; }
};

#endif // MOCHI_NOW_H
