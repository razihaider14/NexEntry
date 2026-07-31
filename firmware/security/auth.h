#pragma once
// ---------------------------------------------------------------------------
// auth.h — administrative MQTT command authentication.
//
// Every protected command topic (unlock, enroll, delete card, enable OTA,
// factory reset, open provisioning portal) MUST include, alongside its
// normal fields:
//
//   {
//     "ts": 1732900000,          // unix timestamp, seconds
//     "nonce": "a1b2c3d4e5f6...", // random per-message, >=16 hex chars
//     "sig": "<hex hmac-sha256>", // see canonical string below
//     ... command-specific fields ...
//   }
//
// The signature is HMAC-SHA256(key = cmdSecret, msg = canonical string):
//   canonical = topic + "|" + ts + "|" + nonce + "|" + compact_body
// where compact_body is the JSON payload with "sig" removed and re-
// serialized with sorted-by-insertion-order keys (i.e. exactly what was
// sent minus the sig field). Dashboards/Node-RED sign with the same
// cmdSecret provisioned via the captive portal.
//
// This is intentionally NOT applied to RFID scans, attendance, or presence
// events (Req. #7 — those stay unauthenticated / unthrottled).
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <ArduinoJson.h>

namespace Auth {
    void init(const char* secret);

    // Validates ts window + nonce replay + HMAC signature. `canonicalBody`
    // is the raw JSON payload text (as received) with the "sig" field value
    // blanked by the caller before hashing — see command_validator.cpp for
    // the exact call site. Returns false and logs a security event on any
    // failure (expired timestamp, replayed nonce, bad signature).
    bool verify(const char* topic, uint32_t ts, const char* nonce,
                const char* canonicalBody, const char* sigHex);

    // Rate limiting — administrative commands only (Req. #7). Returns false
    // if the topic has exceeded CMD_RATE_LIMIT_MAX within the window.
    bool rateLimitOk(const char* topic);
}
