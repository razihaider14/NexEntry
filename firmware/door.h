// ============================================================
//  door.h  —  Servo door lock control + held-open watchdog
// ============================================================

#pragma once

#include <Arduino.h>

namespace Door {
    void init();
    void unlock(uint32_t durationMs);  // unlocks for durationMs then auto-relocks
    void lock();
    bool isUnlocked();
    bool isHeldOpen();
    bool autoRelockFired();   // returns true once then resets
    void tick();                       // call every loop() — handles auto-relock
                                       // and held-open watchdog
}