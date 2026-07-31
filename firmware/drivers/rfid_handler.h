#pragma once
// RFID reader + card registry driver.
// Card-store logic UNCHANGED from v1. v2 adds a mutex because the registry
// is now mutated from two tasks: RFID Task (scans) and MQTT Task (enroll
// save/edit/delete commands arrive over the network).
#include <Arduino.h>
#include "../config.h"

namespace RFID {
    void     init();

    bool     cardPresent();
    void     readUID(char* out, size_t outLen);   // was String — now char[]

    int      lookupCard(const char* uid);

    CardRecord getCard(int index);
    uint8_t    getCardCount();

    bool     addCard(const CardRecord& record);
    bool     editCard(const char* uid, const char* newName, bool whitelisted, uint32_t tempExpiry);
    bool     deleteCard(const char* uid);

    void     saveRegistry();
    void     loadRegistry();

    void     setEnrollMode(bool on);
    bool     isEnrollMode();
}
