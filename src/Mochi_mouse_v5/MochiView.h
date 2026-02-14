#ifndef MOCHI_VIEW_H
#define MOCHI_VIEW_H

#ifndef MOCHI_VERSION
  #define MOCHI_VERSION "0.0.0--DEV"
#endif

#include <LovyanGFX.hpp>
#include "Settings.h"
#include "MochiState.h" // Per conoscere i tipi di dati

class MochiView {
private:
  LGFX_Sprite* canvas; // Puntatore allo sprite

  void drawBackground() {
    // Disegna solo se necessario o ottimizzato
    for (int i = 0; i < 172; i++) {
      float ratio = (float)i / 172.0;
      int r = (int)((1.0 - ratio) * 255 + ratio * 130);
      int g = (int)((1.0 - ratio) * 160 + ratio * 240);
      int b = (int)((1.0 - ratio) * 200 + ratio * 255);
      canvas->drawFastHLine(0, i, 320, canvas->color565(r, g, b));
    }
  }

  void drawUI(MochiState &state, bool connected) {
    // Icone
    canvas->drawLine(11, 10, 8, 14, K_BG_1);
    canvas->drawLine(8, 14, 12, 14, K_BG_1);
    canvas->drawLine(12, 14, 9, 18, K_BG_1);

    int fx = 8, fy = 20;
    canvas->drawPixel(fx+1, fy+1, K_EYE); canvas->drawPixel(fx+4, fy+1, K_EYE);
    if (state.happy > 50) { 
      canvas->drawPixel(fx+1, fy+4, K_EYE); canvas->drawPixel(fx+4, fy+4, K_EYE);
      canvas->drawFastHLine(fx+2, fy+5, 2, K_EYE);
    } else { 
      canvas->drawPixel(fx+1, fy+6, K_EYE); canvas->drawPixel(fx+4, fy+6, K_EYE);
      canvas->drawFastHLine(fx+2, fy+5, 2, K_EYE);
    }

    // Barre
    canvas->fillRect(18, 12, (int)(state.hunger*2), 3, K_BG_1);
    canvas->fillRect(18, 20, (int)(state.happy*2), 3, K_BG_2);
    
    // Spina
    if (connected) {
      int px = 295, py = 12;
      canvas->fillRect(px, py, 10, 6, K_EYE);
      canvas->fillRect(px+10, py+1, 3, 1, K_EYE);
      canvas->fillRect(px+10, py+4, 3, 1, K_EYE);
      canvas->drawFastHLine(px-3, py+3, 3, K_EYE);
    }

    // Progress bar
    int barWidth = (int)(290 * state.getProgress());
    canvas->fillRoundRect(15, 145, 290, 4, 2, canvas->color565(200, 200, 200)); 
    canvas->fillRoundRect(15, 145, barWidth, 4, 2, 0xADFF);

    // Debug Info
    drawDebugInfo(state);
  }

  void drawDebugInfo(MochiState &state) {
    canvas->setTextColor(K_PROG_FG); // Usiamo l'azzurro di sistema
    canvas->setTextSize(1);
    canvas->setTextDatum(TR_DATUM); // Top Right
    
    // Lo posizioniamo vicino al numero di serie o nell'angolo opposto
    // 310 è la coordinata X (quasi a fine schermo 320), 10 è la Y
    canvas->drawString(state.remoteTime.c_str(), 310, 5);
    
    canvas->setTextDatum(TL_DATUM); // Reset allineamento

    // --- AGGIUNTA: STAMPA VERSIONE IN BASSO A SINISTRA ---
    canvas->setTextSize(1);
    canvas->setTextColor(canvas->color565(100, 100, 100)); // Grigio scuro discreto
    canvas->setCursor(5, 162); // Coordinate: x=5 (margine sx), y=162 (fondo)
    canvas->print("v");
    canvas->print(MOCHI_VERSION);
  }

public:
  MochiView(LGFX_Sprite* spritePtr) {
    canvas = spritePtr;
  }

  // La funzione render ora controlla lo stato
  void render(MochiState &state, int yOff, bool wink, bool connected) {
    drawBackground();
    if (state.isDying) {
      drawGhostMochi(yOff); // (Codice del fantasma che hai già)
    } else {
      drawUI(state, connected);
      
      // Determina parametri in base all'età corrente
      int w = 100, h = 80; // Default Adulto
      uint16_t color = K_WHITE;
      
      if (state.currentAge == BABY) {
        w = 70; h = 55; 
      } else if (state.currentAge == ELDER) {
        color = K_ELDER_BODY;
      }
      // Usa la nuova funzione adattiva
      drawAdaptiveMochi(160, 86 + yOff, w, h, color, state.currentAge, wink, state.isHeartVisible, state.isBubbleVisible, state.bubbleType);
    }

    canvas->pushSprite(0, 0);
  }

  void drawGrowthFrame(float t, AgeStage from, AgeStage to) {
    drawBackground();
    
    int cx = 160; int cy = 86;
    int w_start, h_start, w_end, h_end;
    uint16_t c_start, c_end;

    // Configura parametri di partenza e arrivo
    if (from == BABY && to == ADULT) {
      w_start = 70; h_start = 55; c_start = K_WHITE;
      w_end = 100; h_end = 80; c_end = K_WHITE;
    } else if (from == ADULT && to == ELDER) {
      w_start = 100; h_start = 80; c_start = K_WHITE;
      w_end = 100; h_end = 80; c_end = K_ELDER_BODY;
    }

    // Interpolazione lineare (Lerp)
    int current_w = w_start + (int)((w_end - w_start) * t);
    int current_h = h_start + (int)((h_end - h_start) * t);
    
    // Interpolazione colore (semplificata, blend RGB sarebbe meglio ma complesso)
    uint16_t current_color = (t < 0.5) ? c_start : c_end;

    // Effetto "tremolio" ed "espansione" durante la crescita
    float expansion = sin(t * M_PI) * 10; // Si espande al centro dell'animazione
    current_w += expansion; current_h += expansion / 2;
    int jitter = (random(3) - 1) * 2;

    // Durante la transizione usiamo lo stadio di arrivo per i dettagli, 
    // ma la funzione adattiva gestirà le dimensioni intermedie.
    drawAdaptiveMochi(cx + jitter, cy, current_w, current_h, current_color, to, false, false, false, '.');

    // Effetto "luce" o "fumo" intorno
    if (t > 0.2 && t < 0.8) {
       canvas->drawCircle(cx, cy, (current_w/2) + 15 + expansion, K_PROG_FG);
    }

    canvas->pushSprite(0, 0);
  }

private:
  // --- NUOVA FUNZIONE DISEGNO FANTASMA ---
  void drawGhostMochi(int yOff) {
    int cx = 160;
    int cy = 86 + yOff;

    // Corpo Spettrale (Leggermente trasparente/pallido, senza blush)
    // Usiamo fillRoundRect ma magari facciamo la base un po' ondulata in futuro.
    canvas->fillRoundRect(cx - 50, cy - 40, 100, 80, 35, K_GHOST_BODY);
    
    // Occhi a X (Morte)
    // Occhio SX
    canvas->drawLine(cx - 35, cy - 15, cx - 15, cy + 5, K_GHOST_EYE);
    canvas->drawLine(cx - 15, cy - 15, cx - 35, cy + 5, K_GHOST_EYE);
    // Occhio DX
    canvas->drawLine(cx + 15, cy - 15, cx + 35, cy + 5, K_GHOST_EYE);
    canvas->drawLine(cx + 35, cy - 15, cx + 15, cy + 5, K_GHOST_EYE);
  }

  // Helper per le rughe (Anziano)
  void drawWrinkles(int cx, int eyeY, int spacingX) {
    // Le rughe devono seguire la posizione degli occhi
    int offset = spacingX + 10; 
    
    // Sotto gli occhi
    canvas->drawArc(cx - spacingX, eyeY + 10, 10, 12, 220, 320, K_WRINKLE);
    canvas->drawArc(cx + spacingX, eyeY + 10, 10, 12, 220, 320, K_WRINKLE);
    
    // Zampe di gallina
    canvas->drawLine(cx - offset, eyeY, cx - offset - 6, eyeY - 3, K_WRINKLE);
    canvas->drawLine(cx - offset, eyeY + 2, cx - offset - 6, eyeY + 5, K_WRINKLE);

    canvas->drawLine(cx + offset, eyeY, cx + offset + 6, eyeY - 3, K_WRINKLE);
    canvas->drawLine(cx + offset, eyeY + 2, cx + offset + 6, eyeY + 5, K_WRINKLE);
  }

  // Helper per gli occhi
  void drawEyes(int cx, int cy, int spacingX, int yOffset, int rX, int rY, bool wink, AgeStage stage) {
    
    // Posizione occhi
    int leftEyeX = cx - spacingX;
    int rightEyeX = cx + spacingX;
    int eyeY = cy + yOffset;

    // --- STILE BABY (Ovali + Riflesso) ---
    if (stage == BABY) {
      // Occhio Sinistro
      if (wink) {
        // Arco felice
        canvas->drawArc(leftEyeX, eyeY + (rY/3), rX, rY/3, 180, 360, K_EYE);
        canvas->drawArc(leftEyeX, eyeY + (rY/3) + 1, rX, rY/3, 180, 360, K_EYE);
      } else {
        canvas->fillEllipse(leftEyeX, eyeY, rX, rY, K_EYE);
        // Riflesso proporzionato (20% della larghezza occhio)
        int reflectSize = max(2, (int)(rX * 0.4)); 
        canvas->fillCircle(leftEyeX - (rX/2), eyeY - (rY/2), reflectSize, K_WHITE);
      }

      // Occhio Destro
      canvas->fillEllipse(rightEyeX, eyeY, rX, rY, K_EYE);
      int reflectSize = max(2, (int)(rX * 0.4));
      canvas->fillCircle(rightEyeX - (rX/2), eyeY - (rY/2), reflectSize, K_WHITE);
    } 
    
    // --- STILE ADULTO / ANZIANO (Tondi) ---
    else {
      // Occhio SX
      if (wink) {
        canvas->fillRoundRect(leftEyeX - rX, eyeY, rX*2, max(2, rY/2), 2, K_EYE);
      } else {
        canvas->fillCircle(leftEyeX, eyeY, rX, K_EYE); // rX usato come raggio
      }
      // Occhio DX
      canvas->fillCircle(rightEyeX, eyeY, rX, K_EYE);
    }
  }

  void drawAdaptiveMochi(int cx, int cy, int w, int h, uint16_t bodyColor, AgeStage stage, bool wink, bool heart, bool bubble, char bType) {
    
    // 1. Disegna Corpo
    // Raggio angoli: proporzionato all'altezza, ma più tondeggiante per il baby
    int radius = (stage == BABY) ? (h * 0.45) : (h * 0.40); 
    canvas->fillRoundRect(cx - (w/2), cy - (h/2), w, h, radius, bodyColor);

    // 2. Calcola proporzioni per gli elementi facciali
    int spacingX, yOffset, eyeRx, eyeRy;

    if (stage == BABY) {
       // --- PROPORZIONI BABY ---
       // Occhi più distanti in proporzione alla testa piccola (kawaii ratio)
       spacingX = w * 0.28;  
       // Occhi posizionati leggermente sotto il centro (fronte alta)
       yOffset = h * 0.05;   
       // Occhi GRANDI (ovali)
       eyeRx = w * 0.11;     
       eyeRy = h * 0.18;    
       
       // Guance
       int cheekSpace = w * 0.38;
       int cheekY = yOffset + (h * 0.15);
       canvas->fillCircle(cx - cheekSpace, cy + cheekY, 4, K_BLUSH);
       canvas->fillCircle(cx + cheekSpace, cy + cheekY, 4, K_BLUSH);

    } else {
       // --- PROPORZIONI ADULTO/ANZIANO ---
       // Occhi standard
       spacingX = w * 0.25;
       // Occhi sopra il centro
       yOffset = -(h * 0.12);
       // Occhi più piccoli
       eyeRx = w * 0.05; // Raggio cerchio
       eyeRy = eyeRx;    // Non usato per tondi ma passato per coerenza
       
       // Guance
       int cheekSpace = w * 0.35;
       int cheekY = yOffset + (h * 0.20); // Sotto gli occhi
       if (stage == ELDER) cheekY += 5;   // Anziano ha guance più basse

       canvas->fillCircle(cx - cheekSpace, cy + cheekY, (stage==ELDER?6:8), K_BLUSH);
       canvas->fillCircle(cx + cheekSpace, cy + cheekY, (stage==ELDER?6:8), K_BLUSH);
    }

    // 3. Disegna Occhi (passando i valori calcolati)
    drawEyes(cx, cy, spacingX, yOffset, eyeRx, eyeRy, wink, stage);

    // 4. Rughe (Solo Anziano) - Calcolate relative agli occhi
    if (stage == ELDER) {
       drawWrinkles(cx, cy + yOffset, spacingX);
    }

    // 5. Cuore (Sempre sopra la testa)
    if (heart) {
      int hx = cx + (w/2) - 5;
      int hy = cy - (h/2) - 10;
      canvas->fillCircle(hx-3, hy, 4, K_HEART); 
      canvas->fillCircle(hx+3, hy, 4, K_HEART);
      canvas->fillTriangle(hx-7, hy, hx+7, hy, hx, hy+7, K_HEART);
    }

    if (bubble) {
        drawBubble(cx, cy, w, h, bType);
    }
  }

  void drawBubble(int cx, int cy, int w, int h, char type) {
    int bx = cx - (w / 2) - 10; // Lato sinistro
    int by = cy - (h / 2) - 15;
    
    // Disegno ellisse fumetto
    canvas->fillEllipse(bx, by, 12, 10, K_WHITE);
    canvas->drawEllipse(bx, by, 12, 10, K_EYE);
    
    // Codina del fumetto
    canvas->fillTriangle(bx, by + 8, bx + 5, by + 12, bx + 10, by + 8, K_WHITE);
    canvas->drawLine(bx, by + 8, bx + 5, by + 12, K_EYE);
    canvas->drawLine(bx + 10, by + 8, bx + 5, by + 12, K_EYE);

    // Testo (Punto esclamativo o interrogativo)
    canvas->setTextColor(K_EYE);
    canvas->setTextSize(1);
    canvas->setCursor(bx - 3, by - 4);
    canvas->print(type);
  }
};
#endif