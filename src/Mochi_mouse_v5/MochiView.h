#ifndef MOCHI_VIEW_H
#define MOCHI_VIEW_H

#ifndef MOCHI_VERSION
  #define MOCHI_VERSION "0.0.0--DEV" 
#endif

#include <LovyanGFX.hpp>
#include "Settings.h"
#include "MochiState.h"

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
  void drawUI(MochiState &state, bool connected);
  void drawDebugInfo(MochiState &state);
  void drawGhostMochi(int yOff);
  void drawEgg(int cx, int cy, float animAngle, float crackProgress = 0.0f);
  void drawWrinkles(int cx, int eyeY, int spacingX);
  void drawEyes(int cx, int cy, int spacingX, int yOffset, int rX, int rY, bool wink, AgeStage stage);
  void drawAdaptiveMochi(int cx, int cy, int w, int h, uint16_t bodyColor, AgeStage stage, bool wink, bool heart, bool bubble, char bType);

public:
  MochiView(LGFX_Sprite* c);
  ~MochiView();

  // Metodi principali
  void render(MochiState &state, int yOff, float animAngle, bool wink, bool connected);
  void setBackgroundGradient(uint16_t topHex, uint16_t botHex);
  void drawGrowthFrame(float t, AgeStage from, AgeStage to);
};

#endif // MOCHI_VIEW_H