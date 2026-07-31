#pragma once
#include <Arduino.h>
#include "../config.h"
// Secure HTTP OTA. WebOTA (browser upload) has been removed entirely — the
// device now pulls firmware.bin from a backend URL over HTTPS. OTA remains a
// dedicated, dynamically-created FreeRTOS task that self-deletes when the
// job ends (success or failure).

struct HttpOtaJob {
    char version[OTA_VERSION_MAXLEN];
    char url[OTA_URL_MAXLEN];
    char sha256[OTA_SHA256_HEXLEN];
    char signature[OTA_SIGNATURE_MAXLEN]; // may be empty — see ota_security.h
    bool force; // skip the "already running this version" guard
};

// Validates the job (version present + not a no-op reinstall unless
// force=true) and, if valid, creates the OTA task. Returns false (and
// publishes an OTA "failed" status with a reason) if the job is rejected
// before a task is even created.
bool requestStartHttpOta(const HttpOtaJob& job);
