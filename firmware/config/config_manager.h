#pragma once
// ---------------------------------------------------------------------------
// ConfigManager — replaces hardcoded credentials in config.h (Req. #4).
// All fields are stored in NVS under namespace "nexcfg". Fixed-size char
// arrays are used throughout (Req. #9 — avoid dynamic String / fragmentation).
// ---------------------------------------------------------------------------
#include <Arduino.h>

struct DeviceConfig {
    char     wifiSsid[33];
    char     wifiPassword[65];
    char     mqttHost[65];
    uint16_t mqttPort;
    char     mqttUser[33];
    char     mqttPassword[65];
    char     deviceName[33];
    char     otaUser[33];
    char     otaPassword[65];
    // Generated automatically on first save if empty — used as the HMAC key
    // for administrative MQTT command authentication (security/auth.*).
    char     cmdSecret[65];
    bool     configured;
};

namespace ConfigManager {
    // Loads config from NVS into the in-memory copy. Safe to call repeatedly.
    void load();

    // Persists the in-memory copy to NVS.
    void save();

    // True once a full provisioning cycle has completed at least once.
    bool isConfigured();

    // Read-only accessor to the whole struct (e.g. for the portal to prefill
    // fields, or for MQTT/WiFi tasks to connect).
    const DeviceConfig& get();

    // Bulk setter used by the provisioning portal on submit. Persists
    // immediately and marks configured = true.
    void applyProvisioning(const DeviceConfig& incoming);

    // Individual setters (used sparingly — e.g. rotating OTA creds via an
    // authenticated admin command). Each persists immediately.
    void setDeviceName(const char* name);
    void setOtaCredentials(const char* user, const char* pass);

    // Wipes NVS config namespace and reboots into provisioning on next boot.
    void factoryReset();
}
