#pragma once
// LED/buzzer feedback driver — behaviour UNCHANGED from v1.
namespace Feedback {
    void init();
    void granted();
    void denied();
    void unknown();
    void enrollReady();
    void enrollSaved();
    void demoMode();
    void doorHeldOn();
    void doorHeldOff();
    void tick();
}
