// ============================================================
//  time_manager.cpp
// ============================================================

#include "time_manager.h"
#include "config.h"
#include <time.h>

namespace TimeManager {

    static bool    _demoMode   = false;
    static int32_t _demoOffset = 0;      // cumulative seconds added in demo mode
    static bool    _ntpSynced  = false;

    // ── Public ───────────────────────────────────────────────

    void begin() {
        configTime(NTP_GMT_OFFSET, NTP_DST_OFFSET, NTP_SERVER);

        Serial.print("[TIME] Syncing NTP");
        struct tm timeinfo;
        uint8_t attempts = 0;
        while (!getLocalTime(&timeinfo) && attempts < 20) {
            Serial.print(".");
            delay(500);
            attempts++;
        }

        if (attempts < 20) {
            _ntpSynced = true;
            Serial.println(" OK");
        } else {
            // NTP failed — not fatal, time will be wrong but system still runs
            Serial.println(" FAILED (no NTP, timestamps will be 0)");
        }
    }

    uint32_t now() {
        if (!_ntpSynced) return 0;
        time_t raw;
        time(&raw);
        return (uint32_t)(raw + _demoOffset);
    }

    String formatted() {
        // Returns "HH:MM  DD/MM/YYYY"
        // Used on LCD idle screen
        if (!_ntpSynced) return "??:??  --/--/----";

        time_t raw = (time_t)(now());
        struct tm* t = localtime(&raw);

        char buf[20];
        snprintf(buf, sizeof(buf), "%02d:%02d  %02d/%02d/%04d",
                 t->tm_hour, t->tm_min,
                 t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
        return String(buf);
    }

    String formattedTime() {
        // Returns "HH:MM:SS"
        // Used in MQTT status publishes
        if (!_ntpSynced) return "??:??:??";

        time_t raw = (time_t)(now());
        struct tm* t = localtime(&raw);

        char buf[10];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                 t->tm_hour, t->tm_min, t->tm_sec);
        return String(buf);
    }

    void setDemoMode(bool on) {
        _demoMode = on;
        if (!on) _demoOffset = 0;   // reset offset when leaving demo mode
        Serial.printf("[TIME] Demo mode %s\n", on ? "ON" : "OFF");
    }

    void addDemoOffset(int32_t seconds) {
        if (!_demoMode) return;
        _demoOffset += seconds;

        // Print what the demo time is now so you can verify in Serial Monitor
        time_t raw = (time_t)(now());
        struct tm* t = localtime(&raw);
        Serial.printf("[TIME] Demo offset +%ds → demo time now %02d:%02d:%02d\n",
                      seconds, t->tm_hour, t->tm_min, t->tm_sec);
    }

    bool isDemoMode() {
        return _demoMode;
    }

} // namespace TimeManager