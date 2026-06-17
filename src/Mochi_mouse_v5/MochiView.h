#ifndef MOCHI_VIEW_H
#define MOCHI_VIEW_H

#ifndef MOCHI_VERSION
  #define MOCHI_VERSION "0.0.0--DEV"
#endif

#include <LovyanGFX.hpp>
#include "Settings.h"
#include "MochiState.h"
#include "MochiMinigame.h"

class MochiView {
private:
  LGFX_Sprite* canvas;

  LGFX_Sprite* eggTempSprite;

  uint16_t bgColors[172]; 
  bool bgCalculated;
  uint16_t currentBgTop;
  uint16_t currentBgBottom;

  // Helper privati
  void drawBackground();
  void drawBubble(int cx, int cy, int w, int h, char type);
  void drawUI(MochiState &state, bool connected, float animAngle);
  void drawDebugInfo(MochiState &state);
  void drawGhostMochi(int yOff);
  void drawVisitorSign(MochiState &state); // Cartello "TORNO SUBITO" quando il Mochi è via
  void drawVisitScene(MochiState &state);  // Host + ospite che giocano insieme
  void drawEgg(int cx, int cy, float animAngle, float crackProgress = 0.0f);
  void drawWrinkles(int cx, int eyeY, int spacingX);
  void drawEyes(int cx, int cy, int spacingX, int yOffset, int rX, int rY, bool wink, AgeStage stage);
  // bodyColor2/gradient: se gradient==true il corpo è riempito con un gradiente
  // verticale da bodyColor (alto) a bodyColor2 (basso) — usato per l'ospite.
  void drawAdaptiveMochi(int cx, int cy, int w, int h, uint16_t bodyColor, AgeStage stage, bool wink, bool heart, bool bubble, char bType, bool gradient = false, uint16_t bodyColor2 = 0);

  // Helper colore/gradiente
  uint16_t lerp565(uint16_t a, uint16_t b, float t);
  void fillRoundRectGradient(int x, int y, int w, int h, int r, uint16_t top, uint16_t bottom);

  // Minigame helpers
  void drawMgTitle(int x, const char* title);
  void drawMgMochi(int cx, int cy, int w, int h, bool mouthOpen, bool bigEyes);
  void drawMgChew    (MochiMinigame &mg, unsigned long now);
  void drawMgSurprise(MochiMinigame &mg, unsigned long now);
  void drawMgMash    (MochiMinigame &mg, unsigned long now);
  void drawMgReact   (MochiMinigame &mg, unsigned long now);
  void drawMgCount   (MochiMinigame &mg, unsigned long now);
  void drawMgHold    (MochiMinigame &mg, unsigned long now);

public:
  MochiView(LGFX_Sprite* c);
  ~MochiView();

  // Metodi principali
  void render(MochiState &state, int yOff, float animAngle, bool wink, bool connected);
  void setBackgroundColors(uint16_t top, uint16_t bottom);
  void drawGrowthFrame(float t, AgeStage from, AgeStage to);
  void drawMinigame(MochiMinigame &mg, unsigned long now);
  void drawMinigameResult(bool success);
};

#endif // MOCHI_VIEW_H