#include "auth.h"
#include "../config.h"
#include <mbedtls/md.h>
#include <time.h>

namespace Auth {

    static char _secret[65] = {0};

    // ── Replay protection: ring buffer of recently-seen nonces ────────────
    struct NonceEntry {
        char     nonce[40];
        uint32_t seenAt;
        bool     used;
    };
    static NonceEntry _nonceCache[CMD_NONCE_CACHE_SIZE];
    static uint8_t    _nonceCacheIdx = 0;

    // ── Rate limiting (admin commands only) ────────────────────────────────
    struct RateEntry {
        char     topic[48];
        uint32_t windowStart;
        uint8_t  count;
        bool     used;
    };
    static RateEntry _rateTable[8];

    void init(const char* secret) {
        strlcpy(_secret, secret, sizeof(_secret));
        memset(_nonceCache, 0, sizeof(_nonceCache));
        memset(_rateTable, 0, sizeof(_rateTable));
        Serial.println("[AUTH] Initialized command authentication");
    }

    static bool _nonceSeen(const char* nonce) {
        for (auto& e : _nonceCache) {
            if (e.used && strcmp(e.nonce, nonce) == 0) return true;
        }
        return false;
    }

    static void _rememberNonce(const char* nonce) {
        NonceEntry& e = _nonceCache[_nonceCacheIdx];
        strlcpy(e.nonce, nonce, sizeof(e.nonce));
        e.seenAt = (uint32_t)time(nullptr);
        e.used   = true;
        _nonceCacheIdx = (_nonceCacheIdx + 1) % CMD_NONCE_CACHE_SIZE;
    }

    static void _hmacSha256Hex(const char* key, const char* msg, char* outHex /* 65 bytes */) {
        uint8_t digest[32];
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        mbedtls_md_context_t ctx;
        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, info, 1 /* hmac */);
        mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key, strlen(key));
        mbedtls_md_hmac_update(&ctx, (const unsigned char*)msg, strlen(msg));
        mbedtls_md_hmac_finish(&ctx, digest);
        mbedtls_md_free(&ctx);

        static const char* hex = "0123456789abcdef";
        for (int i = 0; i < 32; i++) {
            outHex[i * 2]     = hex[(digest[i] >> 4) & 0xF];
            outHex[i * 2 + 1] = hex[digest[i] & 0xF];
        }
        outHex[64] = '\0';
    }

    static bool _constTimeEq(const char* a, const char* b) {
        size_t la = strlen(a), lb = strlen(b);
        if (la != lb) return false;
        uint8_t diff = 0;
        for (size_t i = 0; i < la; i++) diff |= (uint8_t)(a[i] ^ b[i]);
        return diff == 0;
    }

    bool verify(const char* topic, uint32_t ts, const char* nonce,
                const char* canonicalBody, const char* sigHex) {

        if (_secret[0] == '\0') {
            Serial.println("[AUTH] No secret configured — rejecting all admin commands");
            return false;
        }

        uint32_t nowS = (uint32_t)time(nullptr);
        if (nowS != 0) { // only enforce if NTP has synced; see time_manager
            int32_t skew = (int32_t)(nowS - ts);
            if (skew > CMD_MAX_CLOCK_SKEW_S || skew < -CMD_MAX_CLOCK_SKEW_S) {
                Serial.printf("[AUTH] Rejected %s — timestamp skew %ds\n", topic, skew);
                return false;
            }
        }

        if (strlen(nonce) < 8) {
            Serial.printf("[AUTH] Rejected %s — nonce too short\n", topic);
            return false;
        }
        if (_nonceSeen(nonce)) {
            Serial.printf("[AUTH] Rejected %s — replayed nonce\n", topic);
            return false;
        }

        char canonical[600];
        snprintf(canonical, sizeof(canonical), "%s|%u|%s|%s", topic, ts, nonce, canonicalBody);

        char expectedHex[65];
        _hmacSha256Hex(_secret, canonical, expectedHex);

        if (!_constTimeEq(expectedHex, sigHex)) {
            Serial.printf("[AUTH] Rejected %s — bad signature\n", topic);
            return false;
        }

        _rememberNonce(nonce);
        return true;
    }

    bool rateLimitOk(const char* topic) {
        uint32_t nowMs = millis();

        RateEntry* slot = nullptr;
        for (auto& e : _rateTable) {
            if (e.used && strcmp(e.topic, topic) == 0) { slot = &e; break; }
        }
        if (!slot) {
            for (auto& e : _rateTable) {
                if (!e.used) { slot = &e; break; }
            }
            if (!slot) slot = &_rateTable[0]; // table full — reuse oldest slot
            strlcpy(slot->topic, topic, sizeof(slot->topic));
            slot->windowStart = nowMs;
            slot->count = 0;
            slot->used = true;
        }

        if (nowMs - slot->windowStart >= CMD_RATE_LIMIT_WINDOW_MS) {
            slot->windowStart = nowMs;
            slot->count = 0;
        }

        slot->count++;
        if (slot->count > CMD_RATE_LIMIT_MAX) {
            Serial.printf("[AUTH] Rate limit exceeded for %s\n", topic);
            return false;
        }
        return true;
    }
}
