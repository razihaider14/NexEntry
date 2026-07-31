#include "task_wifi.h"
#include "tasks_common.h"
#include "task_provisioning.h"
#include "../config/config_manager.h"
#include "../services/time_manager.h"
#include <WiFi.h>
#include <esp_task_wdt.h>

// Core 1. Connects WiFi from ConfigManager creds, keeps it alive, and is the
// trigger point for the ">2 minutes down -> open provisioning" recovery
// mode (Req. #6.2). Also kicks off the one-time NTP sync once first
// connected.

static void _taskWifi(void*) {
    esp_task_wdt_add(NULL);

    const DeviceConfig& cfg = ConfigManager::get();
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSsid, cfg.wifiPassword);
    Serial.printf("[WIFI] Connecting to %s\n", cfg.wifiSsid);

    bool     everConnected = false;
    uint32_t downSince     = millis();

    for (;;) {
        esp_task_wdt_reset();

        if (WiFi.status() == WL_CONNECTED) {
            if (!(xEventGroupGetBits(gSystemEvents) & BIT_WIFI_CONNECTED)) {
                Serial.printf("[WIFI] Connected — IP %s\n", WiFi.localIP().toString().c_str());
                xEventGroupSetBits(gSystemEvents, BIT_WIFI_CONNECTED);
            }
            downSince = 0;

            if (!everConnected) {
                everConnected = true;
                TimeManager::begin();
                xEventGroupSetBits(gSystemEvents, BIT_TIME_SYNCED);
            }
        } else {
            xEventGroupClearBits(gSystemEvents, BIT_WIFI_CONNECTED);
            if (downSince == 0) downSince = millis();

            // Req. #6.2 — WiFi unavailable for >2 minutes -> provisioning.
            if (millis() - downSince >= WIFI_DOWN_PROVISION_MS) {
                Serial.println("[WIFI] Down >2min — requesting provisioning portal");
                requestOpenProvisioningPortal();
                downSince = millis(); // avoid re-triggering every loop
            }

            // WiFi.begin() with STA already configured auto-retries; nudge
            // it periodically in case the radio wedged.
            static uint32_t lastRetry = 0;
            if (millis() - lastRetry > 15000) {
                lastRetry = millis();
                WiFi.reconnect();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void taskWifiStart() {
    xTaskCreatePinnedToCore(_taskWifi, "task_wifi", 4096, nullptr, 3, nullptr, 1);
}
