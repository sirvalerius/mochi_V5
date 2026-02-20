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
#define HUNGER_DECAY 0.0005
#define HAPPY_DECAY  0.0003
#define ACTION_INTERVAL 300000 // 5 minuti

// --- RETE WIFI (Nuovo!) ---
#define WIFI_SSID "Mochi_Net"
#define WIFI_PASS "mochi123" // Password (min 8 caratteri)
#define MAX_WIFI_CONN  1  // Limite massimo di dispositivi collegati

#endif