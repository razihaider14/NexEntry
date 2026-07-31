#pragma once
// Presence/attendance logic — UNCHANGED from v1. Guarded with a mutex since
// it's now touched by RFID Task (taps) and MQTT Task (presence/reset cmd).
#include <Arduino.h>
#include "../config.h"

namespace Presence {
    void init();

    bool         processTap(int cardIndex, const char* uid);
    AccessResult getLastResult();

    bool         isInside(int cardIndex);
    uint32_t     getCheckInTime(int cardIndex);

    void         saveState();
    void         loadState();

    void         resetAll();
}
