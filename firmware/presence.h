#pragma once

#include <Arduino.h>
#include "config.h"

namespace Presence {

    void init();

    bool         processTap(int cardIndex, const String& uid);
    AccessResult getLastResult();

    bool         isInside(int cardIndex);
    uint32_t     getCheckInTime(int cardIndex);

    void         saveState();
    void         loadState();

    void         resetAll();   
}