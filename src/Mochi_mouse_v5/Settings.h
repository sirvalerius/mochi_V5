#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// --- PIN HARDWARE (Waveshare S3 LCD) ---
#define PIN_SCLK 40
#define PIN_MOSI 45
#define PIN_DC   41
#define PIN_CS   42
#define PIN_RST  39
#define PIN_BL   48

// --- LED RGB (Nuovo!) ---
#define PIN_RGB       38  // Pin standard per Waveshare ESP32-S3
#define NUM_PIXELS     1

// --- COLORI ---
#define K_WHITE       0xFFFF
#define K_EYE         0x4A49 
#define K_BLUSH       0xFD93
#define K_HEART       0xF800
#define K_BG_1        0xFEE0 // Giallo tenue
#define K_BG_2        0xFABF // Rosa tenue
#define K_PROG_BG     0xC618 // Grigio chiaro
#define K_PROG_FG     0xADFF // Azzurro
#define K_ELDER_BODY  0xCE79 // Un bianco un po' più spento/grigio per l'anziano
#define K_WRINKLE     0x8410 // Un grigio per le rughe d'espressione
#define K_GHOST_BODY  0xE73C // Un bianco/grigio freddo, spettrale
#define K_GHOST_EYE   0x3186 // Grigio scuro per le X

#define K_BG_TOP      0xFD19  // Rosa (Ex: 255, 160, 200)
#define K_BG_BOTTOM   0x879F  // Azzurro (Ex: 130, 240, 255)

// --- IMPOSTAZIONI GRADIENTE SFONDO DI DEFAULT ---
// Colore in alto (Start)
#define DEFAULT_BG_TOP_R 255
#define DEFAULT_BG_TOP_G 160
#define DEFAULT_BG_TOP_B 200

// Colore in basso (End)
#define DEFAULT_BG_BOT_R 130
#define DEFAULT_BG_BOT_G 240
#define DEFAULT_BG_BOT_B 255

// --- LOGICA GIOCO ---
#define MAX_VAL 100.0
#define HUNGER_DECAY 0.17
#define HAPPY_DECAY  0.10
#define ACTION_INTERVAL 300000 // 5 minuti
#define STATE_COOLDOWN  60000  // 1 minuto

// --- BUTTON ---
#define PIN_BTN        0   // BOOT button, active LOW

// --- STATS ---
#define MAX_STAT       99
#define STAT_GAIN      8   // max points earned per successful minigame

// --- SOCIAL / DISCOVERY (ESP-NOW) ---
#define ESPNOW_CHANNEL      1       // Canale WiFi comune a tutti i Mochi per ESP-NOW
#define ANNOUNCE_INTERVAL_MS 5000   // Ogni quanto un Mochi annuncia la propria presenza
#define NEARBY_TIMEOUT_MS   30000   // Dopo quanto un vicino è considerato "sparito"
#define MAX_NEARBY          8       // Numero massimo di vicini tracciati
#define MAX_FRIENDS         16      // Numero massimo di amici memorizzabili

// --- VISITE ---
#define VISIT_DURATION_MS   120000  // Quanto dura una visita (2 min)
#define VISIT_COOLDOWN_MS   600000  // Tempo minimo tra una partenza e l'altra (10 min)
#define VISIT_RSSI_MIN      -75     // Soglia di prossimità per partire (più alto = più vicino)
#define VISIT_CHANCE        25      // Probabilità su 1000 ad ogni check (con amico in range)
#define VISIT_CHECK_MS      15000   // Ogni quanto valutare una possibile partenza
#define VISIT_ACK_TIMEOUT_MS 1500   // Quanto attendere l'ack prima di rinunciare alla partenza

// --- RETE WIFI (Nuovo!) ---
#define WIFI_SSID "Mochi_Net"
#define WIFI_PASS "mochi123" // Password (min 8 caratteri)
#define MAX_WIFI_CONN  1  // Limite massimo di dispositivi collegati

#endif