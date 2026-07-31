#include "task_mqtt.h"
#include "tasks_common.h"
#include "task_provisioning.h"
#include "../services/mqtt_handler.h"
#include <esp_task_wdt.h>

// Core 1. Owns the MQTT connection lifecycle and drains the outbound publish
// queue. MQTT reconnect backoff lives here so a down broker never blocks
// RFID/attendance on Core 0 (Req. #8) — RFID Task pushes to qMqttOut with a
// short timeout and moves on if this task is behind.

static void _taskMqtt(void*) {
    esp_task_wdt_add(NULL);

    // Wait for WiFi before touching the MQTT client.
    xEventGroupWaitBits(gSystemEvents, BIT_WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
    MQTT::init();

    uint32_t lastReconnectAttempt = 0;
    uint32_t mqttDownSince        = 0;
    bool     wasConnected         = false;

    for (;;) {
        esp_task_wdt_reset();

        EventBits_t bits = xEventGroupGetBits(gSystemEvents);
        if (!(bits & BIT_WIFI_CONNECTED)) {
            // WiFi task handles its own recovery; just wait.
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        uint32_t now = millis();
        if (!MQTT::connected()) {
            if (wasConnected) { mqttDownSince = now; wasConnected = false; }
            if (mqttDownSince == 0) mqttDownSince = now;

            if (now - lastReconnectAttempt >= 5000) {
                lastReconnectAttempt = now;
                MQTT::loop(); // attempts one connect
            }

            // Req. #6.3 — MQTT unavailable for >10 minutes triggers recovery.
            if (now - mqttDownSince >= MQTT_DOWN_PROVISION_MS) {
                Serial.println("[MQTT] Down >10min — requesting provisioning portal");
                requestOpenProvisioningPortal();
                mqttDownSince = now; // avoid re-triggering every loop
            }
        } else {
            wasConnected = true;
            mqttDownSince = 0;
            MQTT::loop();
        }

        // Drain outbound publish queue (best-effort — only while connected).
        MqttPublishMsg out;
        while (xQueueReceive(qMqttOut, &out, 0) == pdTRUE) {
            if (MQTT::connected()) {
                MQTT::processOutboundMessage(out);
            }
            // If disconnected, message is dropped rather than blocking
            // (matches Req. #8 — MQTT failure must never affect RFID ops;
            // attendance already persisted to NVS by Presence regardless).
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void taskMqttStart() {
    xTaskCreatePinnedToCore(_taskMqtt, "task_mqtt", 6144, nullptr, 3, nullptr, 1);
}
