#pragma once
// LCD display driver — behaviour UNCHANGED from v1.
// Now driven exclusively via the DisplayMsg queue in tasks/task_display.
#include <Arduino.h>

namespace Display {
    void init();
    void idle();
    void welcome(const char* name);
    void goodbye(const char* name);
    void denied(const char* reason);
    void unknown();
    void enrollMode();
    void enrollScanned(const char* uid);
    void enrollSaved(const char* name);
    void demoModeOn();
    void demoModeOff();
    void updateTime(const char* timeStr);
    void connecting();
    void mqttReconnecting();
    void provisioning();     // new (v2) — captive portal active
    void otaActive();        // new (v2) — WebOTA session active
}
