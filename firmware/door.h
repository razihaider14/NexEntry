#pragma once

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
