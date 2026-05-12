#pragma once

#include <Arduino.h>
#include "config.h"

namespace MQTT {
    void init(void (*onConnectCallback)());  
    void loop();
    bool connected();

    // ── Publishers ──────────────────────────────────────────
    void publishTap(const AccessResult& result);
    void publishAlert(const char* type, const char* uid);
    void publishDoorEvent(const char* event);   
    void publishStatus();
    void publishEnrollScanned(const char* uid);
}
