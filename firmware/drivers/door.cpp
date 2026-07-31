#include "door.h"
#include "../config.h"
#include <ESP32Servo.h>

namespace Door {

    static Servo    _servo;
    static bool     _unlocked         = false;
    static bool     _autoRelockActive = false;
    static bool     _autoRelockFired  = false;
    static uint32_t _unlockTime       = 0;
    static uint32_t _unlockDuration   = 0;
    static bool     _heldAlertSent    = false;

    static void _setLocked(bool locked) {
        if (locked) {
            _servo.write(SERVO_LOCKED_ANGLE);
            _unlocked = false;
            Serial.println("[DOOR] Locked");
        } else {
            _servo.write(SERVO_UNLOCKED_ANGLE);
            _unlocked = true;
            Serial.println("[DOOR] Unlocked");
        }
    }

    void init() {
        _servo.attach(PIN_DOOR);
        _setLocked(true);
        Serial.println("[DOOR] Init OK");
    }

    void unlock(uint32_t durationMs) {
        _setLocked(false);
        _autoRelockActive = true;
        _unlockTime       = millis();
        _unlockDuration   = durationMs;
        _heldAlertSent    = false;
        Serial.printf("[DOOR] Unlock for %dms\n", durationMs);
    }

    void lock() {
        _setLocked(true);
        _autoRelockActive = false;
        _heldAlertSent    = false;
        Serial.println("[DOOR] Manually locked");
    }

    bool isUnlocked() { return _unlocked; }

    void tick() {
        uint32_t now = millis();

        if (_autoRelockActive && _unlocked) {
            if (now - _unlockTime >= _unlockDuration) {
                _setLocked(true);
                _autoRelockActive = false;
                _autoRelockFired = true;
                Serial.println("[DOOR] Auto-relocked");
            }
        }

        if (_unlocked && !_heldAlertSent) {
            if (now - _unlockTime >= DOOR_HELD_OPEN_MS) {
                _heldAlertSent = true;
                Serial.println("[DOOR] HELD-OPEN threshold exceeded");
            }
        }
    }

    bool isHeldOpen() { return _heldAlertSent; }

    bool autoRelockFired() {
        if (_autoRelockFired) { _autoRelockFired = false; return true; }
        return false;
    }
}
