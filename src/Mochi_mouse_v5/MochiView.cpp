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
  } else if (state.isAway) {
    // Il Mochi è in visita altrove: niente pet, solo il cartello "TORNO SUBITO".
    drawUI(state, connected, animAngle);
    drawVisitorSign(state);
  } else {
    drawUI(state, connected, animAngle);

    if (state.currentAge == EGG) {
        drawEgg(160, 86, animAngle);
    } else {
        int w = 100, h = 80;
        uint16_t color = K_WHITE;
        if (state.currentAge == BABY)  { w = 70; h = 55; }
        else if (state.currentAge == ELDER) { color = K_ELDER_BODY; }
        drawAdaptiveMochi(160, 86 + yOff, w, h, color, state.currentAge, wink, state.isHeartVisible, state.isBubbleVisible, state.bubbleType);
    }

    // Ospite in visita: avatar più piccolo affiancato.
    if (state.isHostingGuest) {
        drawGuest(state);
    }

    // Pending action indicator: pulsing banner at bottom.
    // Mostra/lampeggia solo quando il minigame e' di nuovo disponibile
    // (stessa condizione che permette di lanciarlo col bottone).
    if (state.pendingAction != ACTION_NONE && !state.minigamePlayedThisSlot) {
      const char* labels[] = { "", "FEED", "PET", "STR", "SPD", "INT", "CHR" };
      const char* label = labels[(int)state.pendingAction];
      float pulse = (sinf(animAngle * 4.0f) + 1.0f) / 2.0f;
      uint8_t alpha = (uint8_t)(140 + pulse * 115);
      uint16_t bgCol = canvas->color565(0, alpha / 2, alpha);
      canvas->fillRoundRect(80, 148, 160, 16, 4, bgCol);
      canvas->setTextColor(K_WHITE);
      canvas->setTextSize(1);
      canvas->setCursor(88, 153);
      canvas->print("BTN > ");
      canvas->print(label);
    }
  }

  canvas->pushSprite(0, 0);
}

void MochiView::setBackgroundColors(uint16_t top, uint16_t bottom) {
  // Se i colori sono diversi da quelli attuali
  if (currentBgTop != top || currentBgBottom != bottom) {
    currentBgTop = top;
    currentBgBottom = bottom;
    bgCalculated = false; // Questo flag a false FORZA il ricalcolo del gradiente al prossimo render!
  }
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

void MochiView::drawUI(MochiState &state, bool connected, float animAngle) {
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
  
  // Indicatore "amico vicino": due cuoricini affiancati in alto a destra
  if (state.isFriendNearby) {
    int hx = 232, hy = 13;
    for (int s = 0; s < 2; s++) {
      int ox = hx + s * 12;
      canvas->fillCircle(ox,     hy, 2, K_HEART);
      canvas->fillCircle(ox + 4, hy, 2, K_HEART);
      canvas->fillTriangle(ox - 2, hy + 1, ox + 6, hy + 1, ox + 2, hy + 6, K_HEART);
    }
  }

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

// Cartello "TORNO SUBITO" mostrato mentre il Mochi è in visita altrove.
void MochiView::drawVisitorSign(MochiState &state) {
  int cx = 160, cy = 80;

  // Palo del cartello
  canvas->fillRect(cx - 3, cy - 5, 6, 60, K_WRINKLE);

  // Tavola
  canvas->fillRoundRect(cx - 70, cy - 45, 140, 46, 6, K_WHITE);
  canvas->drawRoundRect(cx - 70, cy - 45, 140, 46, 6, K_EYE);

  canvas->setTextColor(K_EYE);
  canvas->setTextSize(2);
  canvas->setCursor(cx - 30, cy - 38); canvas->print("TORNO");
  canvas->setCursor(cx - 36, cy - 20); canvas->print("SUBITO");

  // Countdown al rientro
  unsigned long now = millis();
  long rem = (state.awayUntil > now) ? (long)((state.awayUntil - now) / 1000) : 0;
  canvas->setTextSize(1);
  canvas->setTextColor(canvas->color565(90, 90, 90));
  canvas->setCursor(cx - 40, cy + 22);
  canvas->print("rientro tra ");
  canvas->print(rem);
  canvas->print("s");
}

// Avatar dell'ospite (il Mochi di un amico in visita), affiancato più piccolo.
void MochiView::drawGuest(MochiState &state) {
  AgeStage st = state.guestAge;
  uint16_t color = (st == ELDER) ? K_ELDER_BODY : K_WHITE;
  int gx = 255, gy = 120;

  // Un eventuale ospite "uovo" lo disegniamo comunque come cucciolo.
  drawAdaptiveMochi(gx, gy, 50, 40, color, (st == EGG ? BABY : st), false, false, false, '.');

  canvas->setTextSize(1);
  canvas->setTextColor(canvas->color565(90, 90, 90));
  canvas->setCursor(gx - 18, gy - 34);
  canvas->print("ospite");
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
  eggTempSprite->pushRotateZoom(canvas, cx, anchorY, wobbleDeg, 1.0f, 1.0f, 0xF81F);
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

  if (stage == BABY || stage == ADULT) {
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

// ==========================================
// MINIGAME RENDERING
// ==========================================

void MochiView::drawMgTitle(int x, const char* title) {
  canvas->setTextColor(K_EYE);
  canvas->setTextSize(1);
  canvas->setCursor(x, 3);
  canvas->print(title);
}

// Compact Mochi used in all minigame screens
void MochiView::drawMgMochi(int cx, int cy, int w, int h, bool mouthOpen, bool bigEyes) {
  canvas->fillRoundRect(cx - w/2, cy - h/2, w, h, h * 0.4, K_WHITE);

  int eyeRx = bigEyes ? (w * 0.13) : (w * 0.09);
  int eyeRy = bigEyes ? (h * 0.22) : (h * 0.16);
  int sx    = w * 0.26;
  int eyeY  = cy - (int)(h * 0.07);

  canvas->fillEllipse(cx - sx, eyeY, eyeRx, eyeRy, K_EYE);
  canvas->fillEllipse(cx + sx, eyeY, eyeRx, eyeRy, K_EYE);

  canvas->fillCircle(cx - (int)(w * 0.38), cy + (int)(h * 0.10), 3, K_BLUSH);
  canvas->fillCircle(cx + (int)(w * 0.38), cy + (int)(h * 0.10), 3, K_BLUSH);

  if (mouthOpen) {
    canvas->fillEllipse(cx, cy + (int)(h * 0.22), (int)(w * 0.12), (int)(h * 0.10), K_EYE);
  }
}

// ---- Dispatch ----
void MochiView::drawMinigame(MochiMinigame &mg, unsigned long now) {
  drawBackground();
  switch (mg.type) {
    case MG_CHEW:     drawMgChew    (mg, now); break;
    case MG_SURPRISE: drawMgSurprise(mg, now); break;
    case MG_MASH:     drawMgMash    (mg, now); break;
    case MG_REACT:    drawMgReact   (mg, now); break;
    case MG_COUNT:    drawMgCount   (mg, now); break;
    case MG_HOLD:     drawMgHold    (mg, now); break;
    default: break;
  }
  canvas->pushSprite(0, 0);
}

void MochiView::drawMinigameResult(bool success) {
  drawBackground();
  canvas->setTextSize(3);
  if (success) {
    canvas->setTextColor(canvas->color565(60, 200, 60));
    canvas->setCursor(115, 55);
    canvas->print("NICE!");
    // heart
    int cx = 160, cy = 115;
    canvas->fillCircle(cx - 10, cy, 10, K_HEART);
    canvas->fillCircle(cx + 10, cy, 10, K_HEART);
    canvas->fillTriangle(cx - 20, cy, cx + 20, cy, cx, cy + 18, K_HEART);
  } else {
    canvas->setTextColor(canvas->color565(220, 70, 70));
    canvas->setCursor(108, 55);
    canvas->print("MISS!");
    int cx = 160, cy = 110;
    canvas->drawLine(cx - 14, cy - 14, cx + 14, cy + 14, K_EYE);
    canvas->drawLine(cx + 14, cy - 14, cx - 14, cy + 14, K_EYE);
  }
  canvas->pushSprite(0, 0);
}

// ---- CHEW ----
void MochiView::drawMgChew(MochiMinigame &mg, unsigned long now) {
  unsigned long elapsed = now - mg.startTime;
  float beatPhase = (float)(elapsed % 700UL) / 700.0f;

  drawMgTitle(127, "CHEW!");

  drawMgMochi(95, 86, 52, 40, mg.chewMouthOpen, false);

  // Pulsing food item (rice ball)
  int fr = 12 + (int)(sinf(beatPhase * (float)M_PI) * 4);
  canvas->fillCircle(220, 86, fr, K_BG_1);
  canvas->fillCircle(213, 81, 3, canvas->color565(190, 155, 70));
  canvas->fillCircle(224, 91, 2, canvas->color565(190, 155, 70));

  // Arrow when mouth is open
  if (mg.chewMouthOpen) {
    canvas->fillTriangle(138, 82, 138, 90, 129, 86, K_EYE);
    for (int i = 0; i < 4; i++) canvas->drawFastHLine(141 + i*9, 86, 6, K_EYE);
  }

  // Hit counter top-right
  canvas->setTextColor(canvas->color565(60, 200, 60)); canvas->setCursor(255, 3);
  canvas->print(mg.chewHits); canvas->setTextColor(K_EYE); canvas->print("/8");

  float t = min(1.0f, (float)elapsed / (8UL * 700UL));
  canvas->fillRoundRect(15, 156, 290, 6, 3, K_PROG_BG);
  canvas->fillRoundRect(15, 156, (int)(290.0f * t), 6, 3, K_PROG_FG);
}

// ---- SURPRISE ----
void MochiView::drawMgSurprise(MochiMinigame &mg, unsigned long now) {
  drawMgTitle(112, "SURPRISE!");

  // Box (hiding place)
  int bx = 120, bw = 80, by = 98, bh = 50;
  canvas->fillRoundRect(bx, by, bw, bh, 8, canvas->color565(200, 170, 130));
  canvas->drawRoundRect(bx, by, bw, bh, 8, canvas->color565(150, 120, 80));
  canvas->fillRoundRect(bx + 5, by + 5, bw - 10, 8, 3, canvas->color565(240, 210, 160));

  if (mg.surprisePeeking) {
    // Mochi peeking above the box
    drawMgMochi(160, 82, 50, 38, false, true);
    canvas->setTextSize(2);
    canvas->setTextColor(K_HEART);
    canvas->setCursor(215, 78); canvas->print("!");
    canvas->setCursor(229, 72); canvas->print("!");
  }

  // Peek indicators top-right
  for (int i = 0; i < 3; i++) {
    uint16_t col = (i < mg.surprisePeeks)
      ? (i < mg.surpriseHits ? canvas->color565(60,200,60) : canvas->color565(200,60,60))
      : K_PROG_BG;
    canvas->fillCircle(275 + i * 14, 10, 5, col);
  }
}

// ---- MASH ----
void MochiView::drawMgMash(MochiMinigame &mg, unsigned long now) {
  drawMgTitle(127, "MASH!");

  // Mochi bounces based on tap count
  int bounce = (mg.mashCount % 2 == 0) ? 0 : -8;
  drawMgMochi(160, 76 + bounce, 52, 40, false, false);

  // Large tap count
  canvas->setTextSize(4);
  canvas->setTextColor(K_PROG_FG);
  int numX = (mg.mashCount < 10) ? 148 : (mg.mashCount < 100) ? 130 : 112;
  canvas->setCursor(numX, 108);
  canvas->print(mg.mashCount);

  // Countdown bar
  unsigned long elapsed = now - mg.startTime;
  float t = 1.0f - min(1.0f, (float)elapsed / 5000.0f);
  canvas->fillRoundRect(15, 156, 290, 6, 3, K_PROG_BG);
  canvas->fillRoundRect(15, 156, (int)(290.0f * t), 6, 3, K_BG_2);
}

// ---- REACT ----
void MochiView::drawMgReact(MochiMinigame &mg, unsigned long now) {
  drawMgTitle(120, "REACT!");

  drawMgMochi(160, 80, 52, 40, false, !mg.reactWaiting);

  canvas->setTextSize(2);
  if (mg.reactDone) {
    canvas->setTextColor(canvas->color565(60, 200, 60));
    canvas->setCursor(100, 115);
    canvas->print(mg.reactMs);
    canvas->print(" ms");
  } else if (mg.reactWaiting) {
    canvas->setTextColor(canvas->color565(140, 140, 140));
    canvas->setCursor(104, 115); canvas->print("WAIT...");
  } else {
    canvas->setTextColor(K_HEART);
    canvas->setCursor(104, 115); canvas->print("TAP!!!");
  }
}

// ---- COUNT ----
void MochiView::drawMgCount(MochiMinigame &mg, unsigned long now) {
  drawMgTitle(120, "COUNT!");

  if (mg.countShowing) {
    unsigned long elapsed = now - mg.startTime;
    int bounceIdx = (int)(elapsed / 500UL);
    float beatPh  = (float)(elapsed % 500UL) / 500.0f;
    int yOff = (bounceIdx < mg.countTarget)
              ? -(int)(sinf(beatPh * (float)M_PI) * 22)
              : 0;
    drawMgMochi(160, 82 + yOff, 52, 40, false, false);

    for (int i = 0; i < mg.countTarget; i++) {
      uint16_t col = (i < bounceIdx) ? K_PROG_FG : K_PROG_BG;
      canvas->fillCircle(248 + i * 13, 10, 5, col);
    }
    canvas->setTextSize(2);
    canvas->setTextColor(canvas->color565(150,150,150));
    canvas->setCursor(120, 118); canvas->print("watch...");
  } else {
    drawMgMochi(160, 80, 52, 40, false, false);
    canvas->setTextSize(3);
    canvas->setTextColor(K_PROG_FG);
    canvas->setCursor(118, 108); canvas->print(mg.countPlayer);
    canvas->setTextSize(1);
    canvas->setTextColor(K_EYE);
    canvas->setCursor(118, 138); canvas->print("taps  (wait 2s to confirm)");
  }
}

// ---- HOLD ----
void MochiView::drawMgHold(MochiMinigame &mg, unsigned long now) {
  drawMgTitle(127, "HOLD!");

  drawMgMochi(90, 86, 52, 40, false, false);

  // Horizontal bar: x 155 to 295, y 79-93 (h=14)
  int bx = 155, bw = 140, by = 79, bh = 14;
  canvas->fillRoundRect(bx, by, bw, bh, 4, K_PROG_BG);

  int filled = (int)(mg.holdProgress * bw);
  if (filled > 0) {
    canvas->fillRoundRect(bx, by, filled, bh, 4, K_PROG_FG);
  }

  // Sweet spot zone (tolerance ±15%)
  int ssX   = bx + (int)(mg.holdTarget * bw);
  int tolPx = (int)(0.15f * bw);
  canvas->fillRect(ssX - tolPx, by, tolPx * 2, bh, canvas->color565(255, 200, 60));
  canvas->drawFastVLine(ssX, by - 3, bh + 6, canvas->color565(220, 100, 30));

  canvas->setTextSize(1);
  canvas->setTextColor(K_EYE);
  if (!mg.holdActive) {
    canvas->setCursor(165, 103); canvas->print("hold the button");
  } else if (mg.holdDone) {
    canvas->setCursor(180, 103); canvas->print("released!");
  }
}