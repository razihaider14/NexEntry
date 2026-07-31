#include "task_ota.h"
#include "tasks_common.h"
#include "../config/config_manager.h"
#include "../config/tls_cert.h"
#include "../security/ota_security.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_task_wdt.h>

// Dynamic task (Req: "HTTP OTA should remain a dedicated FreeRTOS task").
// Created only after an authenticated START_HTTP_OTA MQTT command passes
// version validation in requestStartHttpOta(). Self-deletes on completion —
// success (after which it never returns; ESP.restart() ends the device
// before vTaskDelete would run) or failure.

static void _publishOtaStatus(const char* status, int16_t progress = -1) {
    MqttPublishMsg m{};
    m.type = MqttPublishType::OTA_STATUS;
    strlcpy(m.text, status, sizeof(m.text));
    m.otaProgress = progress;
    sendWithTimeout(qMqttOut, &m);
}

// Very small "x.y.z" comparator — good enough to detect "same version" /
// reject a no-op reinstall; not a full semver implementation.
static bool _versionsEqual(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

static void _taskOta(void* pv) {
    HttpOtaJob* job = (HttpOtaJob*)pv;
    esp_task_wdt_add(NULL);

    xEventGroupSetBits(gSystemEvents, BIT_OTA_ACTIVE);
    DisplayMsg dm{DisplayMsgType::OTA_ACTIVE};
    sendWithTimeout(qDisplay, &dm);
    _publishOtaStatus("started", 0);
    Serial.printf("[OTA] Starting HTTP OTA — version=%s url=%s\n", job->version, job->url);

    bool ok = false;
    const char* failReason = "unknown";

    do {
        WiFiClientSecure client;
        client.setCACert(CA_CERT);
        // Certificate-pinning hook: call client.setFingerprint("...") or
        // client.setInsecure()-then-manual-check here instead of setCACert
        // if you want to pin a specific leaf/intermediate cert rather than
        // trust the CA chain. Left as CA-based validation by default.

        HTTPClient http;
        http.setConnectTimeout(OTA_HTTP_CONNECT_TIMEOUT_MS);
        http.setTimeout(OTA_HTTP_TOTAL_TIMEOUT_MS);

        if (!http.begin(client, job->url)) {
            failReason = "could not begin HTTPS connection"; break;
        }

        const DeviceConfig& cfg = ConfigManager::get();
        if (cfg.otaUser[0] != '\0') {
            // Repurposed field (see ota_security.h) — Basic Auth toward the
            // firmware backend, not a browser upload form.
            http.setAuthorization(cfg.otaUser, cfg.otaPassword);
        }
        // Mutual TLS hook: WiFiClientSecure::setCertificate()/setPrivateKey()
        // here if your backend requires a client cert, once one is
        // provisioned onto the device (not currently part of ConfigManager).

        Serial.println("[OTA] Connecting...");
        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("[OTA] HTTP GET failed: %d\n", httpCode);
            failReason = "HTTP request failed"; http.end(); break;
        }

        int contentLength = http.getSize();
        if (contentLength <= 0) {
            failReason = "server did not report a firmware size"; http.end(); break;
        }
        Serial.printf("[OTA] Downloading %d bytes\n", contentLength);
        _publishOtaStatus("downloading", 0);

        if (!Update.begin(contentLength)) {
            Update.printError(Serial);
            failReason = "not enough free space for update"; http.end(); break;
        }
        OtaSecurity::beginFirmwareHash(job->sha256);

        WiFiClient* stream = http.getStreamPtr();
        uint8_t  buf[1024];
        int      written = 0;
        int      lastReportedPct = -1;
        uint32_t lastByteAt = millis();

        while (http.connected() && written < contentLength) {
            esp_task_wdt_reset();
            size_t avail = stream->available();
            if (avail == 0) {
                if (millis() - lastByteAt > OTA_HTTP_CONNECT_TIMEOUT_MS) {
                    failReason = "stalled download (no data)"; break;
                }
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            size_t toRead = min(avail, sizeof(buf));
            int n = stream->readBytes(buf, toRead);
            if (n <= 0) continue;
            lastByteAt = millis();

            OtaSecurity::updateFirmwareHash(buf, n);
            if (Update.write(buf, n) != (size_t)n) {
                Update.printError(Serial);
                failReason = "flash write error"; break;
            }
            written += n;

            int pct = (written * 100) / contentLength;
            if (pct >= lastReportedPct + OTA_PROGRESS_REPORT_STEP_PCT) {
                lastReportedPct = pct;
                _publishOtaStatus("downloading", pct);
                Serial.printf("[OTA] Progress: %d%%\n", pct);
            }
        }
        http.end();

        if (written != contentLength) {
            if (failReason == nullptr || strcmp(failReason, "unknown") == 0) failReason = "incomplete download";
            Update.abort();
            break;
        }

        _publishOtaStatus("verifying", 100);
        if (!OtaSecurity::finishAndVerifyFirmwareHash()) {
            Update.abort();
            failReason = "SHA-256 mismatch — firmware rejected"; break;
        }

        uint8_t digest[32];
        OtaSecurity::getLastDigest(digest);
        if (!OtaSecurity::verifyFirmwareSignature(digest, job->signature)) {
            Update.abort();
            failReason = "signature verification failed — firmware rejected"; break;
        }

        _publishOtaStatus("installing", 100);
        if (!Update.end(true)) {
            Update.printError(Serial);
            failReason = "flash commit failed"; break;
        }

        ok = true;
    } while (0);

    xEventGroupClearBits(gSystemEvents, BIT_OTA_ACTIVE);

    if (ok) {
        Serial.println("[OTA] Update complete — rebooting into new firmware");
        _publishOtaStatus("completed", 100);
        vTaskDelay(pdMS_TO_TICKS(500)); // let the publish flush
        delete job;
        ESP.restart();
        // unreachable
    }

    Serial.printf("[OTA] FAILED: %s\n", failReason);
    _publishOtaStatus(failReason);
    MqttPublishMsg secEvt{};
    secEvt.type = MqttPublishType::SECURITY_EVENT;
    strlcpy(secEvt.alertType, "access/cmd/ota/enable", sizeof(secEvt.alertType));
    strlcpy(secEvt.text, failReason, sizeof(secEvt.text));
    sendWithTimeout(qMqttOut, &secEvt);

    DisplayMsg idleMsg{DisplayMsgType::IDLE};
    sendWithTimeout(qDisplay, &idleMsg);

    delete job;
    vTaskDelete(NULL); // OTA task self-destructs on failure, matching the brief
}

bool requestStartHttpOta(const HttpOtaJob& job) {
    if (xEventGroupGetBits(gSystemEvents) & BIT_OTA_ACTIVE) {
        Serial.println("[OTA] Already active — ignoring duplicate START_HTTP_OTA");
        _publishOtaStatus("failed: OTA already in progress");
        return false;
    }
    if (job.version[0] == '\0') {
        _publishOtaStatus("failed: missing version");
        return false;
    }
    if (job.url[0] == '\0') {
        _publishOtaStatus("failed: missing url");
        return false;
    }
    if (!job.force && _versionsEqual(job.version, FIRMWARE_VERSION)) {
        Serial.printf("[OTA] Rejected — already running version %s (set force:true to reinstall)\n", FIRMWARE_VERSION);
        _publishOtaStatus("failed: version already installed");
        return false;
    }

    HttpOtaJob* heapJob = new HttpOtaJob(job);
    xTaskCreatePinnedToCore(_taskOta, "task_ota", 8192, heapJob, 2, nullptr, 1);
    return true;
}
