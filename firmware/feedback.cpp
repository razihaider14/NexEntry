#include "feedback.h"
#include "config.h"

namespace Feedback {

    static bool     _doorHeldActive    = false;
    static uint32_t _doorHeldLastTick  = 0;
    static bool     _doorHeldLedState  = false;
    #define DOOR_HELD_TICK_MS 400       

    static void _beep(uint32_t durationMs) {
        digitalWrite(PIN_BUZZER, HIGH);
        delay(durationMs);
        digitalWrite(PIN_BUZZER, LOW);
    }

    static void _green(uint32_t durationMs) {
        digitalWrite(PIN_LED_GREEN, HIGH);
        delay(durationMs);
        digitalWrite(PIN_LED_GREEN, LOW);
    }

    static void _red(uint32_t durationMs) {
        digitalWrite(PIN_LED_RED, HIGH);
        delay(durationMs);
        digitalWrite(PIN_LED_RED, LOW);
    }

    static void _allOff() {
        digitalWrite(PIN_LED_GREEN, LOW);
        digitalWrite(PIN_LED_RED,   LOW);
        digitalWrite(PIN_BUZZER,    LOW);
    }

    void init() {
        pinMode(PIN_LED_GREEN, OUTPUT);
        pinMode(PIN_LED_RED,   OUTPUT);
        pinMode(PIN_BUZZER,    OUTPUT);
        _allOff();
        Serial.println("[FEEDBACK] Init OK");
    }

    void granted() {
        _allOff();
        digitalWrite(PIN_LED_GREEN, HIGH);
        _beep(100);
        delay(900);
        digitalWrite(PIN_LED_GREEN, LOW);
    }

    void denied() {
        _allOff();
        digitalWrite(PIN_LED_RED, HIGH);
        _beep(800);
        delay(200);
        digitalWrite(PIN_LED_RED, LOW);
    }

    void unknown() {
        _allOff();
        digitalWrite(PIN_LED_RED, HIGH);
        for (uint8_t i = 0; i < 3; i++) {
            _beep(100);
            delay(150);
        }
        digitalWrite(PIN_LED_RED, LOW);
    }

    void enrollReady() {
        _allOff();
        for (uint8_t i = 0; i < 2; i++) {
            digitalWrite(PIN_LED_GREEN, HIGH);
            digitalWrite(PIN_LED_RED,   LOW);
            _beep(100);
            delay(150);
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_RED,   HIGH);
            delay(150);
        }
        _allOff();
    }

    void enrollSaved() {
        _allOff();
        for (uint8_t i = 0; i < 2; i++) {
            _green(100);
            delay(100);
        }
    }

    void demoMode() {
        _allOff();
        _green(200);
    }

    void doorHeldOn() {
        _doorHeldActive   = true;
        _doorHeldLastTick = millis();
        _doorHeldLedState = false;
        Serial.println("[FEEDBACK] Door held-open pattern START");
    }

    void doorHeldOff() {
        _doorHeldActive = false;
        _allOff();
        Serial.println("[FEEDBACK] Door held-open pattern STOP");
    }

    void tick() {
        if (!_doorHeldActive) return;

        uint32_t now = millis();
        if (now - _doorHeldLastTick < DOOR_HELD_TICK_MS) return;
        _doorHeldLastTick = now;

        _doorHeldLedState = !_doorHeldLedState;
        digitalWrite(PIN_LED_GREEN, _doorHeldLedState ? HIGH : LOW);
        digitalWrite(PIN_LED_RED,  _doorHeldLedState ? LOW  : HIGH);

        if (_doorHeldLedState) {
            digitalWrite(PIN_BUZZER, HIGH);
            delay(50);
            digitalWrite(PIN_BUZZER, LOW);
        }
    }
}