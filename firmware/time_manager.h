// ============================================================
//  time_manager.h  —  Real time + Demo mode time management
// ============================================================

#pragma once

#include <Arduino.h>

namespace TimeManager {
    void     begin();                        // sync NTP, must be called after WiFi connects
    uint32_t now();                          // current unix timestamp (real or demo-offset)
    String   formatted();                    // "HH:MM  DD/MM/YYYY"
    String   formattedTime();               // "HH:MM:SS"  (for status publishes)
    void     setDemoMode(bool on);
    void     addDemoOffset(int32_t seconds); // called from MQTT cmd/time
    bool     isDemoMode();
}