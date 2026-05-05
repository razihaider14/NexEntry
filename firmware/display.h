// ============================================================
//  display.h  —  I2C LCD message management
// ============================================================

#pragma once

#include <Arduino.h>

namespace Display {
    void init();
    void idle();                            // "Scan Card...  " + time on line 2
    void welcome(const char* name);         // "Welcome!"       + name on line 2
    void goodbye(const char* name);         // "Goodbye!"       + name on line 2
    void denied(const char* reason);        // "Access Denied"  + reason on line 2
    void unknown();                         // "Unknown Card"   + "Not Registered"
    void enrollMode();                      // "Enroll Mode"    + "Scan a card..."
    void enrollScanned(const char* uid);    // "Card Scanned!"  + uid on line 2
    void enrollSaved(const char* name);     // "Card Saved!"    + name on line 2
    void demoModeOn();                      // "Demo Mode ON"   + "Time overridden"
    void demoModeOff();                     // "Demo Mode OFF"  + "Real time active"
    void updateTime(const String& timeStr); // updates line 2 on idle screen only
    void connecting();                      // "Connecting..."  + "Please wait"
    void mqttReconnecting();                // "MQTT..."        + "Reconnecting"
}