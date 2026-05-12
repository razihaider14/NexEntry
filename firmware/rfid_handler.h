#pragma once

#include <Arduino.h>
#include "config.h"

namespace RFID {

    void     init();

    bool     cardPresent();
    String   readUID();

    int      lookupCard(const String& uid);   

    CardRecord  getCard(int index);
    uint8_t     getCardCount();

    bool     addCard(const CardRecord& record);     
    bool     editCard(const String& uid, const char* newName, bool whitelisted, uint32_t tempExpiry);         
    bool     deleteCard(const String& uid);        

    void     saveRegistry();
    void     loadRegistry();

    void     setEnrollMode(bool on);
    bool     isEnrollMode();
}