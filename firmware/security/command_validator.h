#pragma once
// ---------------------------------------------------------------------------
// CommandValidator — the single gate every administrative MQTT command must
// pass through before its handler runs. Wraps Auth::verify + rateLimitOk and
// extracts ts/nonce/sig from the parsed JSON so callers don't touch crypto.
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <ArduinoJson.h>

namespace CommandValidator {
    // `topic`  — the MQTT topic the message arrived on.
    // `doc`    — already-parsed JSON (mutable — "sig" is temporarily removed
    //            to compute the canonical body, then restored).
    // Returns true if the command is authentic, fresh, non-replayed, and
    // under the admin rate limit. Publishes a TOPIC_EVT_SECURITY message on
    // any rejection (handled by caller via the returned reason string).
    bool isAuthorized(const char* topic, JsonDocument& doc, char* reasonOut, size_t reasonLen);
}
