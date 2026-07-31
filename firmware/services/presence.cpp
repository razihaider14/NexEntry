#include "presence.h"
#include "../drivers/rfid_handler.h"
#include "time_manager.h"
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace Presence {

    static PersonState  _state[MAX_CARDS];
    static AccessResult _lastResult;
    static Preferences  _prefs;
    static SemaphoreHandle_t _mutex = nullptr;

    static bool _isExpired(const CardRecord& card) {
        if (card.tempExpiry == 0) return false;
        return TimeManager::now() > card.tempExpiry;
    }

    static bool _isLate(uint32_t timestamp) {
        time_t raw = (time_t)timestamp;
        struct tm* t = localtime(&raw);
        if (t->tm_hour > LATE_HOUR) return true;
        if (t->tm_hour == LATE_HOUR && t->tm_min > LATE_MINUTE) return true;
        return false;
    }

    static void _buildDenied(int cardIndex, const char* uid, const char* name, const char* reason) {
        _lastResult.cardIndex = cardIndex;
        strncpy(_lastResult.uid,    uid,    sizeof(_lastResult.uid)    - 1);
        strncpy(_lastResult.name,   name,   sizeof(_lastResult.name)   - 1);
        strncpy(_lastResult.action, "NONE", sizeof(_lastResult.action) - 1);
        strncpy(_lastResult.access, reason, sizeof(_lastResult.access) - 1);
        _lastResult.uid[sizeof(_lastResult.uid)       - 1] = '\0';
        _lastResult.name[sizeof(_lastResult.name)     - 1] = '\0';
        _lastResult.action[sizeof(_lastResult.action) - 1] = '\0';
        _lastResult.access[sizeof(_lastResult.access) - 1] = '\0';
        _lastResult.timestamp = TimeManager::now();
        _lastResult.isLate    = false;
    }

    void init() {
        if (!_mutex) _mutex = xSemaphoreCreateMutex();
        memset(_state,      0, sizeof(_state));
        memset(&_lastResult, 0, sizeof(_lastResult));
        loadState();
        Serial.println("[PRESENCE] Init OK");
    }

    bool processTap(int cardIndex, const char* uid) {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        uint32_t now = TimeManager::now();

        if (cardIndex == -1) {
            _buildDenied(-1, uid, "Unknown", "UNKNOWN");
            xSemaphoreGive(_mutex);
            Serial.printf("[PRESENCE] Unknown card: %s\n", uid);
            return false;
        }

        CardRecord card = RFID::getCard(cardIndex);

        if (!card.whitelisted) {
            _buildDenied(cardIndex, uid, card.name, "DENIED_BLACKLIST");
            xSemaphoreGive(_mutex);
            Serial.printf("[PRESENCE] Blacklisted: %s\n", card.name);
            return false;
        }

        if (_isExpired(card)) {
            _buildDenied(cardIndex, uid, card.name, "DENIED_EXPIRED");
            xSemaphoreGive(_mutex);
            Serial.printf("[PRESENCE] Expired access: %s\n", card.name);
            return false;
        }

        bool wasInside = _state[cardIndex].isInside;
        bool nowInside = !wasInside;
        _state[cardIndex].isInside = nowInside;
        if (nowInside) _state[cardIndex].checkInTime = now;

        _lastResult.cardIndex = cardIndex;
        strncpy(_lastResult.uid,  uid,      sizeof(_lastResult.uid)  - 1);
        strncpy(_lastResult.name, card.name, sizeof(_lastResult.name) - 1);
        _lastResult.uid[sizeof(_lastResult.uid)   - 1] = '\0';
        _lastResult.name[sizeof(_lastResult.name) - 1] = '\0';

        if (nowInside) {
            strncpy(_lastResult.action, "CHECK_IN", sizeof(_lastResult.action) - 1);
            _lastResult.isLate = _isLate(now);
        } else {
            strncpy(_lastResult.action, "CHECK_OUT", sizeof(_lastResult.action) - 1);
            _lastResult.isLate = false;
            _state[cardIndex].checkInTime = 0;
        }
        _lastResult.action[sizeof(_lastResult.action) - 1] = '\0';
        strncpy(_lastResult.access, "GRANTED", sizeof(_lastResult.access) - 1);
        _lastResult.access[sizeof(_lastResult.access) - 1] = '\0';
        _lastResult.timestamp = now;

        xSemaphoreGive(_mutex);
        saveState();

        Serial.printf("[PRESENCE] %s — %s (%s)%s\n", _lastResult.action, card.name, uid, _lastResult.isLate ? " [LATE]" : "");
        return true;
    }

    AccessResult getLastResult() {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        AccessResult r = _lastResult;
        xSemaphoreGive(_mutex);
        return r;
    }

    bool isInside(int cardIndex) {
        if (cardIndex < 0 || cardIndex >= MAX_CARDS) return false;
        xSemaphoreTake(_mutex, portMAX_DELAY);
        bool v = _state[cardIndex].isInside;
        xSemaphoreGive(_mutex);
        return v;
    }

    uint32_t getCheckInTime(int cardIndex) {
        if (cardIndex < 0 || cardIndex >= MAX_CARDS) return 0;
        xSemaphoreTake(_mutex, portMAX_DELAY);
        uint32_t v = _state[cardIndex].checkInTime;
        xSemaphoreGive(_mutex);
        return v;
    }

    void saveState() {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _prefs.begin(NVS_NAMESPACE, false);
        _prefs.putBytes(NVS_KEY_PRESENCE, _state, sizeof(_state));
        _prefs.end();
        xSemaphoreGive(_mutex);
    }

    void loadState() {
        _prefs.begin(NVS_NAMESPACE, true);
        size_t len = _prefs.getBytesLength(NVS_KEY_PRESENCE);
        if (len == sizeof(_state)) {
            _prefs.getBytes(NVS_KEY_PRESENCE, _state, sizeof(_state));
            Serial.println("[PRESENCE] State loaded from NVS");
        } else {
            memset(_state, 0, sizeof(_state));
            Serial.println("[PRESENCE] No state in NVS — everyone set to OUT");
        }
        _prefs.end();
    }

    void resetAll() {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        memset(_state, 0, sizeof(_state));
        xSemaphoreGive(_mutex);
        saveState();
        Serial.println("[PRESENCE] All presence reset to OUT");
    }
}
