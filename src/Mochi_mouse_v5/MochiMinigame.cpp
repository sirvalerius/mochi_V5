#include "MochiMinigame.h"

// ---- Timing constants ----
#define CHEW_BEATS          8
#define CHEW_INTERVAL_MS    700UL
#define CHEW_WINDOW_MS      220UL   // tap must land within first 440ms of each 700ms beat

#define SURPRISE_TOTAL      3
#define SURPRISE_PEEK_MS    500UL
#define SURPRISE_WIN_HITS   2

#define MASH_DURATION_MS    5000UL
#define MASH_WIN_TAPS       10

#define REACT_TIMEOUT_MS    1500UL

#define COUNT_BEAT_MS       500UL
#define COUNT_INPUT_MAX_MS  8000UL

#define HOLD_MAX_MS         2000UL
#define HOLD_TOLERANCE      0.15f   // ±15% of full bar = success

// ================================================================
// Core
// ================================================================

void MochiMinigame::finish(bool s, int sc) {
    success  = s;
    score    = constrain(sc, 0, 100);
    complete = true;
}

void MochiMinigame::begin(MinigameType t, unsigned long now) {
    type      = t;
    startTime = now;
    introEnd  = now + 100;
    complete  = false;
    success   = false;
    score     = 0;

    chewHits = 0; chewBeat = -1; chewMouthOpen = false; chewBeatHit = false;

    surpriseHits = 0; surprisePeeks = 0;
    surprisePeeking = false; surpriseTapped = false;
    surprisePeekEnd = 0;
    surpriseNextPeek = now + 1400;

    mashCount = 0;

    reactWaiting = true; reactDone = false; reactMs = 9999;
    reactSignalTime = now + (unsigned long)random(1000, 3000);

    countTarget = random(2, 6);
    countPlayer = 0; countShowing = true;
    countInputStart = 0; countLastTap = 0;

    holdTarget   = random(25, 76) / 100.0f;
    holdProgress = 0.0f;
    holdActive   = false;
    holdDone     = false;
    holdStartTime = 0;
}

void MochiMinigame::tick(unsigned long now, bool justPressed, bool held, bool justReleased) {
    if (complete) return;
    bool immune = (now < introEnd);
    bool press  = justPressed && !immune;

    switch (type) {
        case MG_CHEW:     tickChew    (now, press);                              break;
        case MG_SURPRISE: tickSurprise(now, press);                              break;
        case MG_MASH:     tickMash    (now, press);                              break;
        case MG_REACT:    tickReact   (now, press);                              break;
        case MG_COUNT:    tickCount   (now, press);                              break;
        case MG_HOLD:     tickHold    (now, immune ? false : held,
                                           immune ? false : justReleased);      break;
        default: break;
    }
}

// ================================================================
// Chew  — tap in rhythm with each beat (mouth-open window)
// ================================================================
void MochiMinigame::tickChew(unsigned long now, bool press) {
    unsigned long elapsed = now - startTime;
    int beat = (int)(elapsed / CHEW_INTERVAL_MS);

    if (beat >= CHEW_BEATS) {
        finish(chewHits >= 5, (chewHits * 100) / CHEW_BEATS);
        return;
    }

    if (beat != chewBeat) {
        chewBeat    = beat;
        chewBeatHit = false;
    }

    unsigned long beatPhase = elapsed % CHEW_INTERVAL_MS;
    chewMouthOpen = (beatPhase < CHEW_WINDOW_MS * 2);

    if (press && !chewBeatHit && chewMouthOpen) {
        chewHits++;
        chewBeatHit = true;
    }
}

// ================================================================
// Surprise  — tap while Mochi is peeking
// ================================================================
void MochiMinigame::tickSurprise(unsigned long now, bool press) {
    if (surprisePeeks >= SURPRISE_TOTAL) {
        finish(surpriseHits >= SURPRISE_WIN_HITS, (surpriseHits * 100) / SURPRISE_TOTAL);
        return;
    }

    if (surprisePeeking) {
        if (press && !surpriseTapped) {
            surpriseHits++;
            surpriseTapped = true;
        }
        if (now >= surprisePeekEnd) {
            surprisePeeking  = false;
            surprisePeeks++;
            surpriseNextPeek = now + (unsigned long)random(1200, 2500);
        }
    } else if (now >= surpriseNextPeek) {
        surprisePeeking = true;
        surpriseTapped  = false;
        surprisePeekEnd = now + SURPRISE_PEEK_MS;
    }
}

// ================================================================
// Mash  — tap as many times as possible in 5 seconds
// ================================================================
void MochiMinigame::tickMash(unsigned long now, bool press) {
    if (press) mashCount++;
    if (now - startTime >= MASH_DURATION_MS) {
        finish(mashCount >= MASH_WIN_TAPS, min(100, (mashCount * 100) / 20));
    }
}

// ================================================================
// React  — tap the instant you see the signal; tapping early fails
// ================================================================
void MochiMinigame::tickReact(unsigned long now, bool press) {
    if (reactDone) return;

    if (reactWaiting) {
        if (now >= reactSignalTime) reactWaiting = false;
        if (press) { reactDone = true; finish(false, 0); } // too early
        return;
    }

    if (press) {
        reactMs   = (int)(now - reactSignalTime);
        reactDone = true;
        finish(reactMs < 1000, max(0, 100 - reactMs / 10));
        return;
    }

    if (now - reactSignalTime > REACT_TIMEOUT_MS) {
        reactDone = true;
        finish(false, 0);
    }
}

// ================================================================
// Count  — watch Mochi bounce N times, then tap exactly N times
// ================================================================
void MochiMinigame::tickCount(unsigned long now, bool press) {
    unsigned long showEnd = startTime + (unsigned long)countTarget * COUNT_BEAT_MS + 600UL;

    if (countShowing) {
        if (now >= showEnd) {
            countShowing    = false;
            countInputStart = now;
            countLastTap    = now;
        }
        return;
    }

    if (press) {
        countPlayer++;
        countLastTap = now;
        if (countPlayer > countTarget + 1) { finish(false, 0); return; }
    }

    // Commit 2 s after last tap (if player has tapped at least once)
    if (countPlayer > 0 && (now - countLastTap > 2000UL)) {
        bool ok = (countPlayer == countTarget);
        finish(ok, ok ? 100 : max(0, 100 - abs(countPlayer - countTarget) * 33));
        return;
    }

    if (now - countInputStart > COUNT_INPUT_MAX_MS) finish(false, 0);
}

// ================================================================
// Hold  — hold button and release at the sweet-spot marker
// ================================================================
void MochiMinigame::tickHold(unsigned long now, bool held, bool released) {
    if (holdDone) return;

    if (held && !holdActive) {
        holdActive    = true;
        holdStartTime = now;
    }

    if (holdActive) {
        holdProgress = min(1.0f, (float)(now - holdStartTime) / (float)HOLD_MAX_MS);
    }

    bool overshoot = holdActive && (holdProgress >= 1.0f);

    if (released || overshoot) {
        holdDone = true;
        if (!holdActive) { finish(false, 0); return; }
        float diff = fabs(holdProgress - holdTarget);
        bool  hit  = (diff <= HOLD_TOLERANCE);
        int   sc   = hit ? (int)(100 - (diff / HOLD_TOLERANCE) * 50) : 0;
        finish(hit, max(0, sc));
    }
}
