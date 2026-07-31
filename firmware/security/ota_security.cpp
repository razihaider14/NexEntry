#include "ota_security.h"
#include "../config/config_manager.h"
#include <mbedtls/sha256.h>

namespace OtaSecurity {

    static mbedtls_sha256_context _ctx;
    static char     _expectedHex[65] = {0};
    static bool     _checking = false;
    static uint8_t  _lastDigest[32] = {0};

    static bool _constTimeEq(const char* a, const char* b) {
        size_t la = strlen(a), lb = strlen(b);
        if (la != lb) return false;
        uint8_t diff = 0;
        for (size_t i = 0; i < la; i++) diff |= (uint8_t)(a[i] ^ b[i]);
        return diff == 0;
    }

    void beginFirmwareHash(const char* expectedSha256Hex) {
        strlcpy(_expectedHex, expectedSha256Hex, sizeof(_expectedHex));
        _checking = strlen(_expectedHex) == 64;
        if (!_checking) {
            Serial.println("[OTA-SEC] WARNING: no SHA-256 supplied — integrity check skipped");
        }
        // Hash is always computed (even if not checked against anything)
        // so getLastDigest()/signature verification still has something
        // meaningful to work with.
        mbedtls_sha256_init(&_ctx);
        mbedtls_sha256_starts(&_ctx, 0 /* SHA-256, not 224 */);
    }

    void updateFirmwareHash(const uint8_t* data, size_t len) {
        mbedtls_sha256_update(&_ctx, data, len);
    }

    bool finishAndVerifyFirmwareHash() {
        mbedtls_sha256_finish(&_ctx, _lastDigest);
        mbedtls_sha256_free(&_ctx);

        if (!_checking) return true; // nothing to compare against — see warning above

        char actualHex[65];
        static const char* hex = "0123456789abcdef";
        for (int i = 0; i < 32; i++) {
            actualHex[i * 2]     = hex[(_lastDigest[i] >> 4) & 0xF];
            actualHex[i * 2 + 1] = hex[_lastDigest[i] & 0xF];
        }
        actualHex[64] = '\0';

        bool ok = _constTimeEq(actualHex, _expectedHex);
        if (!ok) {
            Serial.printf("[OTA-SEC] SHA-256 mismatch — expected %s got %s\n", _expectedHex, actualHex);
        }
        return ok;
    }

    void getLastDigest(uint8_t out[32]) {
        memcpy(out, _lastDigest, 32);
    }

    bool verifyFirmwareSignature(const uint8_t digest[32], const char* signatureHex) {
        (void)digest;
        if (!signatureHex || signatureHex[0] == '\0') {
            Serial.println("[OTA-SEC] WARNING: no signature supplied — authenticity not verified, only integrity (SHA-256)");
            return true;
        }
        // TODO(production): mbedtls_pk_verify(digest, signatureHex-decoded)
        // against an embedded public key once a signing step exists in the
        // release pipeline. Documented no-op for now — see ota_security.h.
        return true;
    }
}
