#include "time_manager.h"
#include "../config.h"
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace TimeManager {

    static bool     _ntpSynced   = false;

    // Demo Mode state — see time_manager.h for the model. All guarded by
    // _mutex since task_rfid/task_display read `now()` while task_mqtt
    // writes these from admin commands.
    static SemaphoreHandle_t _mutex = nullptr;
    static bool     _demoEnabled  = false;
    static bool     _demoPaused   = false;
    static float    _demoSpeed    = 1.0f;
    static uint32_t _demoBaseReal = 0;      // millis() when base was set
    static uint32_t _demoBaseApparent = 0;  // apparent unix time at that base
    static uint32_t _demoPausedApparent = 0;

    static void _ensureMutex() {
        if (!_mutex) _mutex = xSemaphoreCreateMutex();
    }

    static uint32_t _realNow() {
        if (!_ntpSynced) return 0;
        time_t raw;
        time(&raw);
        return (uint32_t)raw;
    }

    // Must be called with _mutex held.
    static uint32_t _demoNowLocked() {
        if (_demoPaused) return _demoPausedApparent;
        uint32_t elapsedRealMs = millis() - _demoBaseReal;
        double   apparentElapsedS = (elapsedRealMs / 1000.0) * _demoSpeed;
        return _demoBaseApparent + (uint32_t)apparentElapsedS;
    }

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
        if (attempts < 20) { _ntpSynced = true; Serial.println(" OK"); }
        else Serial.println(" FAILED (no NTP, timestamps will be 0)");
    }

    uint32_t now() {
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        uint32_t result = _demoEnabled ? _demoNowLocked() : _realNow();
        xSemaphoreGive(_mutex);
        return result;
    }

    static void _formatUnix(uint32_t ts, char* out, size_t outLen, bool withDate) {
        if (ts == 0) {
            strlcpy(out, withDate ? "??:??  --/--/----" : "??:??:??", outLen);
            return;
        }
        time_t raw = (time_t)ts;
        struct tm* t = localtime(&raw);
        if (withDate) {
            snprintf(out, outLen, "%02d:%02d  %02d/%02d/%04d", t->tm_hour, t->tm_min, t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
        } else {
            snprintf(out, outLen, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
        }
    }

    void formatted(char* out, size_t outLen)     { _formatUnix(now(), out, outLen, true); }
    void formattedTime(char* out, size_t outLen) { _formatUnix(now(), out, outLen, false); }

    void setDemoMode(bool on) {
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        if (on && !_demoEnabled) {
            _demoBaseApparent = _realNow();
            _demoBaseReal     = millis();
            _demoSpeed        = 1.0f;
            _demoPaused       = false;
        }
        _demoEnabled = on;
        xSemaphoreGive(_mutex);
        Serial.printf("[TIME] Demo mode %s\n", on ? "ON" : "OFF");
    }

    bool isDemoMode() {
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        bool v = _demoEnabled;
        xSemaphoreGive(_mutex);
        return v;
    }

    void setDemoDateTime(uint32_t apparentUnixTime) {
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _demoEnabled      = true;
        _demoPaused       = false;
        _demoBaseApparent = apparentUnixTime;
        _demoBaseReal     = millis();
        xSemaphoreGive(_mutex);
        Serial.printf("[TIME] Demo datetime set to unix %u\n", apparentUnixTime);
    }

    void setDemoSpeedMultiplier(float multiplier) {
        if (multiplier <= 0) { Serial.println("[TIME] Ignoring non-positive speed multiplier"); return; }
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        // Re-baseline so the change takes effect from "now", not from the
        // original base (otherwise changing speed mid-flight would also
        // retroactively rescale time already elapsed).
        uint32_t currentApparent = _demoEnabled ? _demoNowLocked() : _realNow();
        _demoBaseApparent = currentApparent;
        _demoBaseReal     = millis();
        _demoSpeed        = multiplier;
        xSemaphoreGive(_mutex);
        Serial.printf("[TIME] Demo speed multiplier set to %.2fx\n", multiplier);
    }

    float getDemoSpeedMultiplier() {
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        float v = _demoSpeed;
        xSemaphoreGive(_mutex);
        return v;
    }

    void fastForward(int32_t seconds) {
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        if (!_demoEnabled) {
            // Match legacy behaviour: fast-forward is a no-op outside Demo
            // Mode (mirrors v1's addDemoOffset(), which required demo mode).
            xSemaphoreGive(_mutex);
            Serial.println("[TIME] fastForward ignored — Demo Mode is off");
            return;
        }
        if (_demoPaused) {
            _demoPausedApparent += seconds;
        } else {
            _demoBaseApparent += seconds;
        }
        xSemaphoreGive(_mutex);
        Serial.printf("[TIME] Demo fast-forward %+ds\n", seconds);
    }

    void pauseDemo() {
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        if (_demoEnabled && !_demoPaused) {
            _demoPausedApparent = _demoNowLocked();
            _demoPaused = true;
        }
        xSemaphoreGive(_mutex);
        Serial.println("[TIME] Demo paused");
    }

    void resumeDemo() {
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        if (_demoEnabled && _demoPaused) {
            _demoBaseApparent = _demoPausedApparent;
            _demoBaseReal     = millis();
            _demoPaused       = false;
        }
        xSemaphoreGive(_mutex);
        Serial.println("[TIME] Demo resumed");
    }

    bool isDemoPaused() {
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        bool v = _demoPaused;
        xSemaphoreGive(_mutex);
        return v;
    }

    void resetDemo() {
        _ensureMutex();
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _demoBaseApparent = _realNow();
        _demoBaseReal     = millis();
        _demoSpeed        = 1.0f;
        _demoPaused       = false;
        xSemaphoreGive(_mutex);
        Serial.println("[TIME] Demo reset to real time (still in Demo Mode)");
    }
}
