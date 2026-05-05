// ============================================================
//  rfid_handler.h  —  RC522 reading, registry, enrollment
// ============================================================

#pragma once

#include <Arduino.h>
#include "config.h"

namespace RFID {

    void     init();

    // ── Card Reading ────────────────────────────────────────
    bool     cardPresent();
    String   readUID();

    // ── Registry Lookup ─────────────────────────────────────
    int      lookupCard(const String& uid);   // returns index, -1 if unknown

    // ── Registry Access ─────────────────────────────────────
    CardRecord  getCard(int index);
    uint8_t     getCardCount();

    // ── Registry Editing (called from mqtt_handler) ──────────
    bool     addCard(const CardRecord& record);      // returns false if registry full
    bool     editCard(const String& uid,
                      const char* newName,
                      bool whitelisted,
                      uint32_t tempExpiry);          // returns false if uid not found
    bool     deleteCard(const String& uid);          // returns false if uid not found

    // ── NVS Persistence ─────────────────────────────────────
    void     saveRegistry();
    void     loadRegistry();

    // ── Enrollment Mode ─────────────────────────────────────
    void     setEnrollMode(bool on);
    bool     isEnrollMode();
}