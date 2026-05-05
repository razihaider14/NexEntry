// ============================================================
//  presence.h  —  IN/OUT state machine and access logic
// ============================================================

#pragma once

#include <Arduino.h>
#include "config.h"

namespace Presence {

    void init();

    // ── Core ────────────────────────────────────────────────
    bool         processTap(int cardIndex, const String& uid);
    AccessResult getLastResult();

    // ── State Queries ───────────────────────────────────────
    bool         isInside(int cardIndex);
    uint32_t     getCheckInTime(int cardIndex);

    // ── NVS Persistence ─────────────────────────────────────
    void         saveState();
    void         loadState();

    // ── Admin Commands (called from mqtt_handler) ────────────
    void         resetAll();   // force everyone to OUT, clear NVS
}