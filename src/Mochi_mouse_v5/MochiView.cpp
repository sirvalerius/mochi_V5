#include "MochiView.h"

// ==========================================
// COSTRUTTORE E METODI PUBBLICI
// ==========================================

MochiView::MochiView(LGFX_Sprite* spritePtr) {
  canvas = spritePtr;
  eggTempSprite = nullptr; // Inizialmente vuoto
  // Inizializza le variabili definite nel file .h
  bgCalculated = false;
  currentBgTop = K_BG_TOP;
  currentBgBottom = K_BG_BOTTOM;
}

MochiView::~MochiView() {
  // Pulisce la memoria se lo sprite dell'uovo è stato creato
  if (eggTempSprite) {
    eggTempSprite->deleteSprite();
    delete eggTempSprite;
  }
}

void MochiView::render(MochiState &state, int yOff, float animAngle, bool wink, bool connected) {
  drawBackground();

  if (state.isDying) {
    drawGhostMochi(yOff); 
  } else {
    drawUI(state, connected);
    
    // Se è un uovo, disegniamo solo l'uovo saltando occhi, rughe e bocca
    if (state.currentAge == EGG) {
        drawEgg(160, 86, animAngle);
    } 
    // Altrimenti procediamo con il Mochi normale
    else {
        int w = 100, h = 80; // Default Adulto
        uint16_t color = K_WHITE;
        
        if (state.currentAge == BABY) {
          w = 70; h = 55; 
        } else if (state.currentAge == ELDER) { color = K_ELDER_BODY; }
        
        drawAdaptiveMochi(160, 86 + yOff, w, h, color, state.currentAge, wink, state.isHeartVisible, state.isBubbleVisible, state.bubbleType);
    }
  }

  canvas->pushSprite(0, 0);
}

void MochiView::setBackgroundGradient(uint16_t topHex, uint16_t botHex) {
  currentBgTop = topHex;
  currentBgBottom = botHex;
  bgCalculated = false; // Forza il ricalcolo al prossimo frame
}

void MochiView::drawGrowthFrame(float t, AgeStage from, AgeStage to) {
  drawBackground();
  int cx = 160; int cy = 86;

  // --- 1. SCHIUSA DELL'UOVO ---
  if (from == EGG && to == BABY) {
      // Tremolio violento che aumenta verso la fine
      float shakeAngle = sin(t * 30.0f) * (t * 15.0f); 

      if (t < 0.8) {
          // L'uovo si crepa gradualmente (passiamo t come crackProgress)
          drawEgg(cx, cy, shakeAngle, t);
      } else {
          // Esplosione di luce bianca finale
          drawEgg(cx, cy, shakeAngle, 1.0f); 
          int flashRadius = (t - 0.8f) * 5.0f * 150.0f; // Il cerchio si espande velocemente
          canvas->fillCircle(cx, cy + 40, flashRadius, K_WHITE);
          
          // Proprio all'ultimo istante, disegna il baby Mochi dentro la luce
          if (t > 0.9) {
              drawAdaptiveMochi(cx, cy, 70, 55, K_WHITE, BABY, false, false, false, '.');
          }
      }
      canvas->pushSprite(0, 0);
      return;
  }

  // --- 2. CRESCITA NORMALE (Baby -> Adult -> Elder) ---
  int w_start, h_start, w_end, h_end;
  uint16_t c_start, c_end;

  if (from == BABY && to == ADULT) {
    w_start = 70; h_start = 55; c_start = K_WHITE;
    w_end = 100; h_end = 80; c_end = K_WHITE;
  } else if (from == ADULT && to == ELDER) {
    w_start = 100; h_start = 80; c_start = K_WHITE;
    w_end = 100; h_end = 80; c_end = K_ELDER_BODY;
  } else {
    // Sicurezza: se ci sono altre transizioni impreviste, usa default
    w_start = 100; h_start = 80; c_start = K_WHITE;
    w_end = 100; h_end = 80; c_end = K_WHITE;
  }

  int current_w = w_start + (int)((w_end - w_start) * t);
  int current_h = h_start + (int)((h_end - h_start) * t);
  uint16_t current_color = (t < 0.5) ? c_start : c_end;

  float expansion = sin(t * M_PI) * 10; 
  current_w += expansion; current_h += expansion / 2;
  int jitter = (random(3) - 1) * 2;

  drawAdaptiveMochi(cx + jitter, cy, current_w, current_h, current_color, to, false, false, false, '.');

  if (t > 0.2 && t < 0.8) {
     canvas->drawCircle(cx, cy, (current_w/2) + 15 + expansion, K_PROG_FG);
  }

  canvas->pushSprite(0, 0);
}


// ==========================================
// METODI PRIVATI (Helper di disegno)
// ==========================================

void MochiView::drawBackground() {
  // Calcola i colori del gradiente SOLO se è cambiato o è il primo avvio
  if (!bgCalculated) {
    
    // 1. "Spacchetta" i colori a 16-bit nei 3 canali R, G, B (0-255)
    uint8_t topR = ((currentBgTop >> 11) & 0x1F) * 255 / 31;
    uint8_t topG = ((currentBgTop >> 5) & 0x3F) * 255 / 63;
    uint8_t topB = (currentBgTop & 0x1F) * 255 / 31;

    uint8_t botR = ((currentBgBottom >> 11) & 0x1F) * 255 / 31;
    uint8_t botG = ((currentBgBottom >> 5) & 0x3F) * 255 / 63;
    uint8_t botB = (currentBgBottom & 0x1F) * 255 / 31;

    for (int i = 0; i < 172; i++) {
      // Usa 171.0 così l'ultima riga (171) raggiunge esattamente 1.0
      float ratio = (float)i / 171.0; 
      
      // Curva a Coseno per transizioni morbide
      float smoothRatio = (1.0 - cos(ratio * PI)) / 2.0;

      // Calcola la sfumatura con i canali spacchettati
      int r = (int)((1.0 - smoothRatio) * topR + smoothRatio * botR);
      int g = (int)((1.0 - smoothRatio) * topG + smoothRatio * botG);
      int b = (int)((1.0 - smoothRatio) * topB + smoothRatio * botB);

      // Ricompatta e salva nell'array
      bgColors[i] = canvas->color565(r, g, b);
    }
    bgCalculated = true; // Array pronto!
  }

  // Disegna lo sfondo alla massima velocità
  for (int i = 0; i < 172; i++) {
    canvas->drawFastHLine(0, i, 320, bgColors[i]);
  }
}

void MochiView::drawUI(MochiState &state, bool connected) {
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

void MochiView::drawDebugInfo(MochiState &state) {
  // --- STAMPA ORA ---
  canvas->setTextColor(K_PROG_FG);
  canvas->setCursor(260, 10);
  // NOTA: Devi assicurarti che getTimeString() esista e restituisca una String in MochiState
  canvas->print(state.getTimeString());

  // --- STAMPA VERSIONE ---
  canvas->setTextSize(1);
  canvas->setTextColor(canvas->color565(100, 100, 100)); // Grigio scuro discreto
  canvas->setCursor(5, 162); 
  canvas->print("v");
  canvas->print(MOCHI_VERSION);
}

void MochiView::drawGhostMochi(int yOff) {
  int cx = 160;
  int cy = 86 + yOff;

  // Corpo Spettrale
  canvas->fillRoundRect(cx - 50, cy - 40, 100, 80, 35, K_GHOST_BODY);
  
  // Occhio SX
  canvas->drawLine(cx - 35, cy - 15, cx - 15, cy + 5, K_GHOST_EYE);
  canvas->drawLine(cx - 15, cy - 15, cx - 35, cy + 5, K_GHOST_EYE);
  // Occhio DX
  canvas->drawLine(cx + 15, cy - 15, cx + 35, cy + 5, K_GHOST_EYE);
  canvas->drawLine(cx + 35, cy - 15, cx + 15, cy + 5, K_GHOST_EYE);
}

void MochiView::drawEgg(int cx, int cy, float animAngle, float crackProgress) {
  int rX = 35; int rY = 45;
  int eggW = (rX * 2) + 10; 
  int eggH = (rY * 2) + 10;
  int anchorY = cy + rY; 

  // Se l'uovo è sano oscilla morbidamente, se si sta rompendo trema violentemente
  float wobbleDeg = (crackProgress > 0) ? animAngle : sin(animAngle) * 6.0f;

  // Variabile statica per ricordare a che punto era la crepa nel frame precedente
  static float lastCrack = -1.0f;

  // Ridisegniamo lo sprite solo se non esiste o se la crepa sta avanzando
  if (eggTempSprite == nullptr || crackProgress != lastCrack) {
      if (eggTempSprite == nullptr) {
          eggTempSprite = new LGFX_Sprite(canvas);
          eggTempSprite->createSprite(eggW, eggH);
      }
      lastCrack = crackProgress;
      
      eggTempSprite->fillScreen(0xF81F); // Sfondo trasparente
      int scx = eggW / 2;
      int scy = eggH / 2;
      
      // Disegna l'uovo base
      eggTempSprite->fillEllipse(scx, scy, rX, rY, K_WHITE);
      eggTempSprite->fillCircle(scx - 15, scy + 10, 5, K_PROG_BG);
      eggTempSprite->fillCircle(scx + 12, scy - 15, 7, K_PROG_BG);
      eggTempSprite->fillCircle(scx + 18, scy + 20, 3, K_PROG_BG);
      eggTempSprite->fillCircle(scx - 8, scy - 25, 4, K_PROG_BG);

      // --- DISEGNO DELLA FRATTURA (Avanza col tempo) ---
      if (crackProgress > 0.1f) {
          int topY = scy - rY + 5;
          eggTempSprite->drawLine(scx, topY, scx - 8, topY + 15, K_WRINKLE);
          eggTempSprite->drawLine(scx+1, topY, scx - 7, topY + 15, K_WRINKLE); // Per spessore
      }
      if (crackProgress > 0.4f) {
          int topY = scy - rY + 5;
          eggTempSprite->drawLine(scx - 8, topY + 15, scx + 8, topY + 30, K_WRINKLE);
          eggTempSprite->drawLine(scx - 7, topY + 15, scx + 9, topY + 30, K_WRINKLE);
      }
      if (crackProgress > 0.7f) {
          int topY = scy - rY + 5;
          eggTempSprite->drawLine(scx + 8, topY + 30, scx - 4, topY + 45, K_WRINKLE);
          eggTempSprite->drawLine(scx + 9, topY + 30, scx - 3, topY + 45, K_WRINKLE);
      }

      eggTempSprite->setPivot(scx, scy + rY);
  }
  // Ombra a terra e disegno Sprite ruotato
  canvas->fillEllipse(cx, anchorY - 5, rX - 5, 8, K_PROG_BG);
  eggTempSprite->pushRotateZO(canvas, cx, anchorY, wobbleDeg, 1.0f, 1.0f, 0xF81F);
}

void MochiView::drawWrinkles(int cx, int eyeY, int spacingX) {
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

void MochiView::drawEyes(int cx, int cy, int spacingX, int yOffset, int rX, int rY, bool wink, AgeStage stage) {
  int leftEyeX = cx - spacingX;
  int rightEyeX = cx + spacingX;
  int eyeY = cy + yOffset;

  if (stage == BABY) {
    if (wink) {
      canvas->drawArc(leftEyeX, eyeY + (rY/3), rX, rY/3, 180, 360, K_EYE);
      canvas->drawArc(leftEyeX, eyeY + (rY/3) + 1, rX, rY/3, 180, 360, K_EYE);
    } else {
      canvas->fillEllipse(leftEyeX, eyeY, rX, rY, K_EYE);
      int reflectSize = max(2, (int)(rX * 0.4)); 
      canvas->fillCircle(leftEyeX - (rX/2), eyeY - (rY/2), reflectSize, K_WHITE);
    }

    canvas->fillEllipse(rightEyeX, eyeY, rX, rY, K_EYE);
    int reflectSize = max(2, (int)(rX * 0.4));
    canvas->fillCircle(rightEyeX - (rX/2), eyeY - (rY/2), reflectSize, K_WHITE);
  } else {
    // STILE ADULTO / ANZIANO
    if (wink) {
      canvas->fillRoundRect(leftEyeX - rX, eyeY, rX*2, max(2, rY/2), 2, K_EYE);
    } else {
      canvas->fillCircle(leftEyeX, eyeY, rX, K_EYE); 
    }
    canvas->fillCircle(rightEyeX, eyeY, rX, K_EYE);
  }
}

void MochiView::drawAdaptiveMochi(int cx, int cy, int w, int h, uint16_t bodyColor, AgeStage stage, bool wink, bool heart, bool bubble, char bType) {
  // 1. Disegna Corpo
  int radius = (stage == BABY) ? (h * 0.45) : (h * 0.40); 
  canvas->fillRoundRect(cx - (w/2), cy - (h/2), w, h, radius, bodyColor);

  // 2. Calcola proporzioni per gli elementi facciali
  int spacingX, yOffset, eyeRx, eyeRy;

  if (stage == BABY) {
     spacingX = w * 0.28;  
     yOffset = h * 0.05;   
     eyeRx = w * 0.11;     
     eyeRy = h * 0.18;    
     
     int cheekSpace = w * 0.38;
     int cheekY = yOffset + (h * 0.15);
     canvas->fillCircle(cx - cheekSpace, cy + cheekY, 4, K_BLUSH);
     canvas->fillCircle(cx + cheekSpace, cy + cheekY, 4, K_BLUSH);

  } else {
     spacingX = w * 0.25;
     yOffset = -(h * 0.12);
     eyeRx = w * 0.05; 
     eyeRy = eyeRx;    
     
     int cheekSpace = w * 0.35;
     int cheekY = yOffset + (h * 0.20); 
     if (stage == ELDER) cheekY += 5;   

     canvas->fillCircle(cx - cheekSpace, cy + cheekY, (stage==ELDER?6:8), K_BLUSH);
     canvas->fillCircle(cx + cheekSpace, cy + cheekY, (stage==ELDER?6:8), K_BLUSH);
  }

  // 3. Disegna Occhi
  drawEyes(cx, cy, spacingX, yOffset, eyeRx, eyeRy, wink, stage);

  // 4. Rughe (Solo Anziano)
  if (stage == ELDER) {
     drawWrinkles(cx, cy + yOffset, spacingX);
  }

  // 5. Cuore
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

void MochiView::drawBubble(int cx, int cy, int w, int h, char type) {
  int bx = cx - (w / 2) - 10; 
  int by = cy - (h / 2) - 15;
  
  // Disegno ellisse fumetto
  canvas->fillEllipse(bx, by, 12, 10, K_WHITE);
  canvas->drawEllipse(bx, by, 12, 10, K_EYE);
  
  // Codina del fumetto
  canvas->fillTriangle(bx, by + 8, bx + 5, by + 12, bx + 10, by + 8, K_WHITE);
  canvas->drawLine(bx, by + 8, bx + 5, by + 12, K_EYE);
  canvas->drawLine(bx + 10, by + 8, bx + 5, by + 12, K_EYE);

  // Testo 
  canvas->setTextColor(K_EYE);
  canvas->setTextSize(1);
  canvas->setCursor(bx - 3, by - 4);
  canvas->print(type);
}