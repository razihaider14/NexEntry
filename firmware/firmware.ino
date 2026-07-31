// =============================================================================
// NexEntry v2 — FreeRTOS firmware
//
// Externally identical to v1 (same RFID/attendance/door/dashboard behaviour,
// same MQTT topics/payloads). Internally: FreeRTOS task architecture,
// Secure HTTP OTA (backend-hosted firmware.bin over HTTPS — no ArduinoOTA,
// no browser-based WebOTA), NVS-backed ConfigManager + WiFiManager
// provisioning, and authenticated administrative MQTT commands.
//
// See /docs in the project root for the architecture explanation, task
// diagram, security audit, and migration notes.
// =============================================================================
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>

#include "config.h"
#include "config/config_manager.h"

#include "tasks/tasks_common.h"
#include "tasks/task_provisioning.h"
#include "tasks/task_wifi.h"
#include "tasks/task_mqtt.h"
#include "tasks/task_status.h"
#include "tasks/task_rfid.h"
#include "tasks/task_door.h"
#include "tasks/task_display.h"
#include "tasks/task_feedback.h"

// ── Globals defined here, declared `extern` in tasks_common.h ─────────────
EventGroupHandle_t gSystemEvents;
QueueHandle_t       qDisplay;
QueueHandle_t       qFeedback;
QueueHandle_t       qDoorCmd;
QueueHandle_t       qMqttOut;

// ── Triple-reset detection (Req. #6.4) ──────────────────────────────────────
// RTC (slow) memory survives a software/watchdog reset but is undefined on a
// true power-on, hence the magic-number guard. A boot is "quick" if it
// happens again before `_clearBootCountTimer` fires.
RTC_NOINIT_ATTR uint32_t _rtcMagic;
RTC_NOINIT_ATTR uint32_t _rtcBootCount;
#define RTC_MAGIC_VALUE 0xACCE55EEu

static bool _checkTripleReset() {
    if (_rtcMagic != RTC_MAGIC_VALUE) {
        _rtcMagic = RTC_MAGIC_VALUE;
        _rtcBootCount = 0;
    }
    _rtcBootCount++;
    Serial.printf("[BOOT] Quick-reset counter: %u\n", _rtcBootCount);

    bool triggered = _rtcBootCount >= TRIPLE_RESET_COUNT;
    if (triggered) _rtcBootCount = 0;
    return triggered;
}

static void _clearBootCountLater(void*) {
    vTaskDelay(pdMS_TO_TICKS(TRIPLE_RESET_WINDOW_MS));
    _rtcBootCount = 0; // this boot was stable — no longer part of a reset burst
    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== NexEntry v2 — Booting ===");
    Serial.printf("Firmware version: %s\n", FIRMWARE_VERSION);

    // Rollback safety hook (HTTP OTA "roll back safely if possible" — see
    // docs/09-HTTP-OTA.md). This confirms the currently-running image is
    // good; if CONFIG_APP_ROLLBACK_ENABLE isn't set in your sdkconfig (the
    // Arduino IDE default), this is a harmless no-op — true automatic
    // rollback-on-crash requires that ESP-IDF option, which isn't reachable
    // from stock Arduino IDE. See docs/09-HTTP-OTA.md for the full caveat.
    esp_ota_mark_app_valid_cancel_rollback();

    // Watchdog (Req. #8) — covers every task via esp_task_wdt_add() in each.
    // NOTE: esp_task_wdt_init()'s signature differs between arduino-esp32
    // core 2.x (esp_task_wdt_init(seconds, panic)) and 3.x
    // (esp_task_wdt_init(&esp_task_wdt_config_t{...})). The call below is
    // the 2.x form — see MIGRATION.md for the 3.x snippet if your board
    // package is on core 3.x.
    esp_task_wdt_init(15, true); // 15s timeout, panic (reboot) on trip

    pinMode(PIN_RESET_BUTTON, INPUT_PULLUP);
    bool buttonHeld  = (digitalRead(PIN_RESET_BUTTON) == LOW);
    bool tripleReset = _checkTripleReset();
    xTaskCreate(_clearBootCountLater, "boot_stable", 1536, nullptr, 1, nullptr);

    gSystemEvents = xEventGroupCreate();
    qDisplay  = xQueueCreate(8,  sizeof(DisplayMsg));
    qFeedback = xQueueCreate(8,  sizeof(FeedbackMsg));
    qDoorCmd  = xQueueCreate(4,  sizeof(DoorCmdMsg));
    qMqttOut  = xQueueCreate(16, sizeof(MqttPublishMsg));

    ConfigManager::load();

    bool needProvisioning = !ConfigManager::isConfigured() || buttonHeld || tripleReset;
    if (needProvisioning) {
        Serial.printf("[BOOT] Entering provisioning — configured=%d button=%d tripleReset=%d\n",
                      ConfigManager::isConfigured(), buttonHeld, tripleReset);
        // Blocking by design: nothing else can usefully run without config,
        // and this call reboots the device on success (Req. #5/#6).
        blockingRunProvisioningPortal();
        // If we reach here, the portal timed out without new config on a
        // device that WAS already configured (button/triple-reset case) —
        // fall through and boot normally with the existing config.
    }

    // ── Core 0: latency-sensitive, user-facing tasks ───────────────────────
    taskRfidStart();
    taskDoorStart();
    taskDisplayStart();
    taskFeedbackStart();

    // ── Core 1: networking tasks ────────────────────────────────────────────
    taskWifiStart();
    taskMqttStart();
    taskStatusStart();

    Serial.println("=== Boot complete — tasks running ===\n");
}

void loop() {
    // All work happens in FreeRTOS tasks; nothing to do here.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
