#pragma once

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
    void updateTime(const String& timeStr);
    void connecting();                     
    void mqttReconnecting();             
}