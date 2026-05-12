#include "time_manager.h"
#include "config.h"
#include <time.h>

namespace TimeManager {

    static bool    _demoMode   = false;
    static int32_t _demoOffset = 0;      
    static bool    _ntpSynced  = false;

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
        if (!_ntpSynced) return "??:??  --/--/----";

        time_t raw = (time_t)(now());
        struct tm* t = localtime(&raw);

        char buf[20];
        snprintf(buf, sizeof(buf), "%02d:%02d  %02d/%02d/%04d", t->tm_hour, t->tm_min, t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
        return String(buf);
    }

    String formattedTime() {
        if (!_ntpSynced) return "??:??:??";

        time_t raw = (time_t)(now());
        struct tm* t = localtime(&raw);

        char buf[10];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
        return String(buf);
    }

    void setDemoMode(bool on) {
        _demoMode = on;
        if (!on) _demoOffset = 0;   
        Serial.printf("[TIME] Demo mode %s\n", on ? "ON" : "OFF");
    }

    void addDemoOffset(int32_t seconds) {
        if (!_demoMode) return;
        _demoOffset += seconds;

        time_t raw = (time_t)(now());
        struct tm* t = localtime(&raw);
        Serial.printf("[TIME] Demo offset +%ds → demo time now %02d:%02d:%02d\n", seconds, t->tm_hour, t->tm_min, t->tm_sec);
    }

    bool isDemoMode() {
        return _demoMode;
    }
} 