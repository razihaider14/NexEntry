#include "presence.h"
#include "rfid_handler.h"
#include "time_manager.h"
#include "config.h"
#include <Preferences.h>

namespace Presence {

    static PersonState  _state[MAX_CARDS];
    static AccessResult _lastResult;
    static Preferences  _prefs;

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

    static void _buildDenied(int cardIndex, const String& uid, const char* name, const char* reason) {
        _lastResult.cardIndex = cardIndex;
        strncpy(_lastResult.uid,    uid.c_str(), sizeof(_lastResult.uid)    - 1);
        strncpy(_lastResult.name,   name,        sizeof(_lastResult.name)   - 1);
        strncpy(_lastResult.action, "NONE",      sizeof(_lastResult.action) - 1);
        strncpy(_lastResult.access, reason,      sizeof(_lastResult.access) - 1);
        _lastResult.uid[sizeof(_lastResult.uid)       - 1] = '\0';
        _lastResult.name[sizeof(_lastResult.name)     - 1] = '\0';
        _lastResult.action[sizeof(_lastResult.action) - 1] = '\0';
        _lastResult.access[sizeof(_lastResult.access) - 1] = '\0';
        _lastResult.timestamp = TimeManager::now();
        _lastResult.isLate    = false;
    }

    void init() {
        memset(_state,      0, sizeof(_state));
        memset(&_lastResult, 0, sizeof(_lastResult));
        loadState();
        Serial.println("[PRESENCE] Init OK");
    }

    bool processTap(int cardIndex, const String& uid) {
        uint32_t now = TimeManager::now();

        if (cardIndex == -1) {
            _buildDenied(-1, uid, "Unknown", "UNKNOWN");
            Serial.printf("[PRESENCE] Unknown card: %s\n", uid.c_str());
            return false;
        }

        CardRecord card = RFID::getCard(cardIndex);

        if (!card.whitelisted) {
            _buildDenied(cardIndex, uid, card.name, "DENIED_BLACKLIST");
            Serial.printf("[PRESENCE] Blacklisted: %s\n", card.name);
            return false;
        }

        if (_isExpired(card)) {
            _buildDenied(cardIndex, uid, card.name, "DENIED_EXPIRED");
            Serial.printf("[PRESENCE] Expired access: %s\n", card.name);
            return false;
        }

        bool wasInside = _state[cardIndex].isInside;
        bool nowInside = !wasInside;

        _state[cardIndex].isInside = nowInside;

        if (nowInside) {
            _state[cardIndex].checkInTime = now;
        } 

        _lastResult.cardIndex = cardIndex;
        strncpy(_lastResult.uid,  uid.c_str(), sizeof(_lastResult.uid)  - 1);
        strncpy(_lastResult.name, card.name,   sizeof(_lastResult.name) - 1);
        _lastResult.uid[sizeof(_lastResult.uid)   - 1] = '\0';
        _lastResult.name[sizeof(_lastResult.name) - 1] = '\0';

        if (nowInside) {
            strncpy(_lastResult.action, "CHECK_IN",  sizeof(_lastResult.action) - 1);
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

        saveState();

        Serial.printf("[PRESENCE] %s — %s (%s)%s\n", _lastResult.action, card.name, uid.c_str(), _lastResult.isLate ? " [LATE]" : "");

        return true;
    }

    AccessResult getLastResult() {
        return _lastResult;
    }

    bool isInside(int cardIndex) {
        if (cardIndex < 0 || cardIndex >= MAX_CARDS) return false;
        return _state[cardIndex].isInside;
    }

    uint32_t getCheckInTime(int cardIndex) {
        if (cardIndex < 0 || cardIndex >= MAX_CARDS) return 0;
        return _state[cardIndex].checkInTime;
    }

    void saveState() {
        _prefs.begin(NVS_NAMESPACE, false);
        _prefs.putBytes(NVS_KEY_PRESENCE, _state, sizeof(_state));
        _prefs.end();
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
        memset(_state, 0, sizeof(_state));
        saveState();
        Serial.println("[PRESENCE] All presence reset to OUT");
    }

}