#ifndef MOCHI_SERVER_H
#define MOCHI_SERVER_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h> // <--- AGGIUNTA IMPORTANTE
#include <Adafruit_NeoPixel.h> 
#include "Settings.h"
#include "MochiState.h"

const byte DNS_PORT = 53;

class MochiServer {
private:
  WebServer server;
  DNSServer dnsServer; // <--- Oggetto DNS
  MochiState* statePtr;
  Adafruit_NeoPixel led; 

  // L'HTML (resta invariato)
  const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Comic Sans MS', sans-serif; text-align: center; background-color: #fef0f5; color: #555; }
    h1 { color: #ff6b6b; }
    .card { background: white; padding: 20px; border-radius: 15px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); max-width: 400px; margin: 20px auto; }
    .btn { display: block; width: 100%; padding: 15px; margin: 10px 0; font-size: 20px; border: none; border-radius: 10px; cursor: pointer; transition: 0.2s; }
    .btn-feed { background-color: #ffd93d; color: #fff; }
    .btn-play { background-color: #6c5ce7; color: #fff; }
    .btn-kill { background-color: #ff4757; color: #fff; } 
    .btn:active { transform: scale(0.95); }
  </style>
</head>
<body>
  <div class="card">
    <h1>Mochi Controller</h1>
    <p>Prenditi cura di me!</p>
    <button class="btn btn-feed" onclick="sendCmd('FEED')">Nutri</button>
    <button class="btn btn-play" onclick="sendCmd('PLAY')">Gioca</button>
    <button class="btn btn-kill" onclick="sendCmd('KILL')">RESET</button>
    <br>
    <button class="btn" onclick="sendCmd('GROW')" style="background-color:#55efc4; color:#fff;">CRESCI (Test)</button>
  </div>
  <script>
    function sendCmd(action) {
      fetch('/cmd?act=' + action).then(resp => {
        console.log('Comando inviato: ' + action);
      });
    }
  </script>
</body>
</html>
)rawliteral";

public:
  MochiServer(MochiState* st) : server(80), led(NUM_PIXELS, PIN_RGB, NEO_GRB + NEO_KHZ800) {
    statePtr = st;
  }

  void begin() {
    // 1. Inizializza LED
    led.begin();
    led.setBrightness(50);
    led.setPixelColor(0, led.Color(0, 0, 0)); 
    led.show();

    // 2. Configura Access Point
    WiFi.softAP(WIFI_SSID, WIFI_PASS, 1, 0, MAX_WIFI_CONN);
    delay(100); // Piccola pausa per stabilità
    
    // Configura il redirect DNS (Cattura tutto il traffico)
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP()); // Stampiamo l'IP per sicurezza

    // 3. Rotte Server
    // Gestione della root
    server.on("/", [this]() {
      server.send(200, "text/html", htmlPage);
    });

    // Gestione comando
    server.on("/cmd", [this]() {
      if (server.hasArg("act")) {
        statePtr->applyCommand(server.arg("act"));
        server.send(200, "text/plain", "OK");
      }
    });

    // Gestione "Captive Portal" (Android/Windows check)
    // Reindirizza qualsiasi pagina non trovata alla root
    server.onNotFound([this]() {
      server.send(200, "text/html", htmlPage);
    });

    server.begin();
  }

  void updateLED() {
    int numStations = WiFi.softAPgetStationNum();
    if (numStations > 0) {
      led.setPixelColor(0, led.Color(0, 255, 0)); // Verde se connesso
    } else {
      led.setPixelColor(0, led.Color(0, 0, 0)); 
    }
    led.show();
  }

  void handle() {
    dnsServer.processNextRequest(); // <--- IMPORTANTE: Gestisce il DNS
    server.handleClient();
    updateLED(); 
  }
};

#endif