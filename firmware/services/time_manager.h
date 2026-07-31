#pragma once
// ---------------------------------------------------------------------------
// Time manager — NTP sync + full Demo Mode.
//
// v2.1: Demo Mode was verified intact from earlier NexEntry v2 work (it
// was NOT dropped) and is expanded here per the new requirements:
// demo_enabled, demo_datetime (set an absolute apparent time), and
// demo_speed_multiplier (time acceleration), plus pause/resume/reset/
// fast-forward controls. All reads/writes are mutex-protected since Demo
// Mode is now touched from task_rfid (Presence::processTap -> now()),
// task_display (idle clock), and task_mqtt (admin commands) concurrently.
// ---------------------------------------------------------------------------
#include <Arduino.h>

namespace TimeManager {
    void     begin(); // NTP sync — call once WiFi is up

    // Apparent time (what Presence/attendance logic sees) — real UTC-ish
    // unix time in production, or the demo clock's current position when
    // Demo Mode is enabled.
    uint32_t now();

    void     formatted(char* out, size_t outLen);       // "HH:MM  DD/MM/YYYY"
    void     formattedTime(char* out, size_t outLen);    // "HH:MM:SS"

    // ── Demo Mode (demo_enabled) ────────────────────────────────────────
    // Enabling with no prior demo_datetime set starts the demo clock at
    // the current real time, running at 1x, unpaused.
    void     setDemoMode(bool on);
    bool     isDemoMode();

    // demo_datetime — jump the apparent clock to an absolute unix
    // timestamp. Implicitly enables Demo Mode and unpauses.
    void     setDemoDateTime(uint32_t apparentUnixTime);

    // demo_speed_multiplier — how fast the apparent clock advances per
    // real second. 1.0 = realtime, 60.0 = "1 second = 1 minute",
    // 3600.0 = "1 second = 1 hour". Only affects the clock while running
    // (not while paused).
    void     setDemoSpeedMultiplier(float multiplier);
    float    getDemoSpeedMultiplier();

    // Fast-forward — nudge the apparent clock forward (or backward with a
    // negative value) by a fixed number of apparent seconds, independent
    // of the speed multiplier. Works whether paused or running. This is
    // also what legacy {"mode":"DEMO","offsetSeconds":N} payloads map to.
    void     fastForward(int32_t seconds);

    // Pause / Resume — freezes/unfreezes the apparent clock in place.
    void     pauseDemo();
    void     resumeDemo();
    bool     isDemoPaused();

    // Reset — re-syncs the apparent demo clock to the current real time
    // and resets the speed multiplier to 1x, without leaving Demo Mode
    // (call setDemoMode(false) separately to exit demo entirely).
    void     resetDemo();
}
