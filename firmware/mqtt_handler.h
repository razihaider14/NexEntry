// ============================================================
//  mqtt_handler.h  —  MQTT publish, subscribe, and dispatch
// ============================================================

#pragma once

#include <Arduino.h>
#include "config.h"

namespace MQTT {
    void init(void (*onConnectCallback)());  // callback fires after every (re)connect
    void loop();
    bool connected();

    // ── Publishers ──────────────────────────────────────────
    void publishTap(const AccessResult& result);
    void publishAlert(const char* type, const char* uid);
    void publishDoorEvent(const char* event);   // "UNLOCKED" | "LOCKED" | "HELD_OPEN"
    void publishStatus();
    void publishEnrollScanned(const char* uid);
}