#include "command_validator.h"
#include "auth.h"

namespace CommandValidator {

    bool isAuthorized(const char* topic, JsonDocument& doc, char* reasonOut, size_t reasonLen) {
        if (!doc["ts"].is<uint32_t>() && !doc["ts"].is<long>()) {
            strlcpy(reasonOut, "missing ts", reasonLen);
            return false;
        }
        if (!doc["nonce"].is<const char*>()) {
            strlcpy(reasonOut, "missing nonce", reasonLen);
            return false;
        }
        if (!doc["sig"].is<const char*>()) {
            strlcpy(reasonOut, "missing sig", reasonLen);
            return false;
        }

        uint32_t    ts    = doc["ts"].as<uint32_t>();
        const char* nonce = doc["nonce"].as<const char*>();
        char sigHex[65];
        strlcpy(sigHex, doc["sig"].as<const char*>(), sizeof(sigHex));

        // Compute canonical body = doc re-serialized with "sig" removed,
        // preserving insertion order (see security/auth.h for rationale).
        doc.remove("sig");
        char canonicalBody[512];
        serializeJson(doc, canonicalBody, sizeof(canonicalBody));

        if (!Auth::rateLimitOk(topic)) {
            strlcpy(reasonOut, "rate limited", reasonLen);
            return false;
        }

        if (!Auth::verify(topic, ts, nonce, canonicalBody, sigHex)) {
            strlcpy(reasonOut, "auth failed", reasonLen);
            return false;
        }

        return true;
    }
}
