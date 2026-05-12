#pragma once

#include <Arduino.h>

namespace TimeManager {
    void     begin();                     
    uint32_t now();                       
    String   formatted();                   
    String   formattedTime();              
    void     setDemoMode(bool on);
    void     addDemoOffset(int32_t seconds);
    bool     isDemoMode();
}
