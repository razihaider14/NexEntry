#pragma once
// ---------------------------------------------------------------------------
// ota_security.h — integrity/authenticity checks for Secure HTTP OTA.
//
// v2.1 change: WebOTA (browser upload) is gone, and with it
// OtaSecurity::checkCredentials() (it existed only to check the Basic Auth
// header on the WebOTA HTML upload form). The OTA username/password fields
// in ConfigManager are *repurposed*, not removed — task_ota now sends them
// as HTTP Basic Auth credentials on the outbound firmware download request,
// for backends that want to gate who can pull firmware.bin. See
// tasks/task_ota.cpp.
// ---------------------------------------------------------------------------
#include <Arduino.h>

namespace OtaSecurity {
    // Call once at the start of a download with the expected SHA-256 (hex,
    // 64 chars) supplied by the MQTT OTA command. Pass an empty string to
    // skip verification (logged as a warning — not recommended for
    // production).
    void beginFirmwareHash(const char* expectedSha256Hex);

    // Feed each received chunk while streaming the download into flash.
    void updateFirmwareHash(const uint8_t* data, size_t len);

    // Call after the download completes, before calling Update.end()/
    // rebooting. Returns false (and the update must be aborted) if a hash
    // was expected and did not match.
    bool finishAndVerifyFirmwareHash();

    // Returns the just-computed SHA-256 digest (32 raw bytes) from the most
    // recent finishAndVerifyFirmwareHash() call — used as the input to
    // verifyFirmwareSignature() below, since we stream firmware straight to
    // flash and never hold the whole image in RAM to sign/verify directly.
    void getLastDigest(uint8_t out[32]);

    // --------------------------------------------------------------------
    // Signature verification hook (Req. #10). Arduino IDE / this ESP32 core
    // do not give us a turnkey secure-boot signing pipeline, so this is a
    // documented integration point: wire in mbedtls_pk_verify() here with an
    // embedded public key once a build-signing step exists in your release
    // pipeline (e.g. a CI job that signs the firmware's SHA-256 digest with
    // an offline private key — signing the digest rather than the raw image
    // is the standard/practical choice here since the image is streamed
    // straight to flash and never buffered whole in RAM).
    //
    // `digest` is the 32-byte SHA-256 already verified against the
    // command's `sha256` field; `signatureHex` is the command's optional
    // `signature` field (hex-encoded), or an empty string if none was
    // supplied. Currently returns true unconditionally regardless of
    // whether a signature was supplied (documented no-op — see rationale
    // above; a missing signature is logged as a warning).
    // --------------------------------------------------------------------
    bool verifyFirmwareSignature(const uint8_t digest[32], const char* signatureHex);
}
