#ifndef MOCHI_MINIGAME_H
#define MOCHI_MINIGAME_H

#include <Arduino.h>

enum MinigameType {
    MG_NONE,
    MG_CHEW,       // FEED
    MG_SURPRISE,   // PET
    MG_MASH,       // STR
    MG_REACT,      // SPD
    MG_COUNT,      // INT
    MG_HOLD        // CHR
};

class MochiMinigame {
public:
    MinigameType  type        = MG_NONE;
    bool          complete    = false;
    bool          success     = false;
    int           score       = 0;       // 0-100, scales stat gain
    unsigned long startTime   = 0;
    unsigned long introEnd    = 0;       // 100ms input immunity after launch

    // --- Chew ---
    int  chewHits     = 0;
    int  chewBeat     = -1;
    bool chewMouthOpen = false;
    bool chewBeatHit  = false;

    // --- Surprise ---
    int  surpriseHits    = 0;
    int  surprisePeeks   = 0;            // peeks shown so far (max 3)
    bool surprisePeeking = false;
    bool surpriseTapped  = false;
    unsigned long surprisePeekEnd  = 0;
    unsigned long surpriseNextPeek = 0;

    // --- Mash ---
    int mashCount = 0;

    // --- React ---
    bool reactWaiting   = true;
    bool reactDone      = false;
    unsigned long reactSignalTime = 0;
    int  reactMs        = 9999;

    // --- Count ---
    int  countTarget    = 3;
    int  countPlayer    = 0;
    bool countShowing   = true;
    unsigned long countInputStart = 0;
    unsigned long countLastTap    = 0;

    // --- Hold ---
    float holdTarget    = 0.5f;
    float holdProgress  = 0.0f;
    bool  holdActive    = false;
    bool  holdDone      = false;
    unsigned long holdStartTime = 0;

    void begin(MinigameType t, unsigned long now);
    void tick(unsigned long now, bool justPressed, bool held, bool justReleased);

private:
    void tickChew    (unsigned long now, bool press);
    void tickSurprise(unsigned long now, bool press);
    void tickMash    (unsigned long now, bool press);
    void tickReact   (unsigned long now, bool press);
    void tickCount   (unsigned long now, bool press);
    void tickHold    (unsigned long now, bool held, bool released);
    void finish(bool s, int sc);
};

#endif
