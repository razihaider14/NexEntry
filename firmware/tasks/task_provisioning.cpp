#include "task_provisioning.h"
#include "tasks_common.h"
#include "../config/config_manager.h"
#include <WiFiManager.h>
#include <esp_task_wdt.h>

// Portal fields (Req. #5): WiFi SSID/Password are collected natively by
// WiFiManager; the rest are added as custom parameters below. Portal
// lifetime is 5 minutes and closes automatically if unused (Req. #6).

static void _runPortal(bool isFirstBoot) {
    WiFiManager wm;
    wm.setConfigPortalTimeout(PROVISION_PORTAL_MS / 1000);
    wm.setBreakAfterConfig(true);

    const DeviceConfig& existing = ConfigManager::get();

    char portMqtt[6];
    snprintf(portMqtt, sizeof(portMqtt), "%u", existing.mqttPort ? existing.mqttPort : 8883);

    WiFiManagerParameter pMqttHost("mqtt_host", "MQTT Host", existing.mqttHost, 64);
    WiFiManagerParameter pMqttPort("mqtt_port", "MQTT Port", portMqtt, 6);
    WiFiManagerParameter pMqttUser("mqtt_user", "MQTT Username", existing.mqttUser, 32);
    WiFiManagerParameter pMqttPass("mqtt_pass", "MQTT Password", existing.mqttPassword, 64);
    WiFiManagerParameter pDevName("dev_name", "Device Name", existing.deviceName[0] ? existing.deviceName : "nexentry", 32);
    WiFiManagerParameter pOtaUser("ota_user", "OTA Username", existing.otaUser[0] ? existing.otaUser : "admin", 32);
    WiFiManagerParameter pOtaPass("ota_pass", "OTA Password", "", 64);

    wm.addParameter(&pMqttHost);
    wm.addParameter(&pMqttPort);
    wm.addParameter(&pMqttUser);
    wm.addParameter(&pMqttPass);
    wm.addParameter(&pDevName);
    wm.addParameter(&pOtaUser);
    wm.addParameter(&pOtaPass);

    xEventGroupSetBits(gSystemEvents, BIT_PROVISIONING);
    DisplayMsg dm{DisplayMsgType::PROVISIONING};
    sendWithTimeout(qDisplay, &dm, pdMS_TO_TICKS(1000));

    Serial.println("[PROVISION] Opening captive portal 'NexEntry-Setup'");
    bool connected;
    if (isFirstBoot) {
        connected = wm.autoConnect("NexEntry-Setup");
    } else {
        // Don't tear down a working STA connection just to offer the
        // portal — startConfigPortal runs AP+STA concurrently.
        connected = wm.startConfigPortal("NexEntry-Setup");
    }

    xEventGroupClearBits(gSystemEvents, BIT_PROVISIONING);

    if (!connected && isFirstBoot) {
        Serial.println("[PROVISION] Portal timed out with no config on first boot — retrying");
        ESP.restart();
        return;
    }
    if (!connected) {
        Serial.println("[PROVISION] Portal closed/timed out — resuming normal operation");
        return;
    }

    DeviceConfig cfg{};
    strlcpy(cfg.wifiSsid,     WiFi.SSID().c_str(), sizeof(cfg.wifiSsid));
    strlcpy(cfg.wifiPassword, WiFi.psk().c_str(),  sizeof(cfg.wifiPassword));
    strlcpy(cfg.mqttHost,     pMqttHost.getValue(), sizeof(cfg.mqttHost));
    cfg.mqttPort = (uint16_t)atoi(pMqttPort.getValue());
    if (cfg.mqttPort == 0) cfg.mqttPort = 8883;
    strlcpy(cfg.mqttUser,     pMqttUser.getValue(), sizeof(cfg.mqttUser));
    strlcpy(cfg.mqttPassword, pMqttPass.getValue(), sizeof(cfg.mqttPassword));
    strlcpy(cfg.deviceName,   pDevName.getValue(),  sizeof(cfg.deviceName));
    strlcpy(cfg.otaUser,      pOtaUser.getValue(),  sizeof(cfg.otaUser));
    // Keep the previous OTA password if the field was left blank.
    const char* newOtaPass = pOtaPass.getValue();
    strlcpy(cfg.otaPassword, (newOtaPass[0] != '\0') ? newOtaPass : existing.otaPassword, sizeof(cfg.otaPassword));
    strlcpy(cfg.cmdSecret, existing.cmdSecret, sizeof(cfg.cmdSecret)); // preserved across reprovision

    ConfigManager::applyProvisioning(cfg);
    Serial.println("[PROVISION] Config saved — rebooting into normal operation");
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP.restart();
}

void blockingRunProvisioningPortal() {
    _runPortal(true);
}

static void _taskProvisioning(void*) {
    esp_task_wdt_add(NULL);
    _runPortal(false);
    vTaskDelete(NULL);
}

void requestOpenProvisioningPortal() {
    if (xEventGroupGetBits(gSystemEvents) & BIT_PROVISIONING) return; // already open
    xTaskCreatePinnedToCore(_taskProvisioning, "task_provision", 6144, nullptr, 2, nullptr, 1);
}
