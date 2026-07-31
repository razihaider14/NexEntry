#pragma once
// Door driver — servo lock/unlock + auto-relock + held-open detection.
// Logic UNCHANGED from v1; now called exclusively from tasks/task_door.
#include <Arduino.h>

namespace Door {
    void init();
    void unlock(uint32_t durationMs);
    void lock();
    bool isUnlocked();
    bool isHeldOpen();
    bool autoRelockFired();
    void tick();
}
