// ============================================================
//  feedback.h  —  LED and buzzer feedback patterns
// ============================================================

#pragma once

namespace Feedback {
    void init();
    void granted();      // green LED + single short beep   → access granted
    void denied();       // red LED + single long beep       → blacklisted / expired
    void unknown();      // red LED + triple short beep      → unregistered card
    void enrollReady();  // alternating LEDs + double beep   → enrollment mode active
    void enrollSaved();  // green LED + double beep          → card saved successfully
    void demoMode();     // single short blue-ish (green) blink, no beep → demo toggled
    void doorHeldOn();   // non-blocking: starts alternating LED + beep pattern
    void doorHeldOff();  // stops the door held pattern
    void tick();         // call every loop() — handles non-blocking door held pattern
}