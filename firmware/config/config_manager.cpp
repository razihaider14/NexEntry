#include "config_manager.h"
#include <Preferences.h>
#include <esp_system.h>

namespace ConfigManager {

    static const char* NS = "nexcfg";
    static Preferences  _prefs;
    static DeviceConfig _cfg;
    static bool         _loaded = false;

    static void _generateSecretIfEmpty() {
        if (_cfg.cmdSecret[0] != '\0') return;

        // Not cryptographically ideal (esp_random is HW TRNG on ESP32 — it IS
        // suitable), but documented here as the exact source: esp_random()
        // pulls from the SAR ADC / RF noise-based HW RNG.
        uint8_t raw[32];
        for (int i = 0; i < 32; i += 4) {
            uint32_t r = esp_random();
            memcpy(&raw[i], &r, 4);
        }
        static const char* hex = "0123456789abcdef";
        for (int i = 0; i < 32 && (i * 2) < (int)sizeof(_cfg.cmdSecret) - 1; i++) {
            _cfg.cmdSecret[i * 2]     = hex[(raw[i] >> 4) & 0xF];
            _cfg.cmdSecret[i * 2 + 1] = hex[raw[i] & 0xF];
        }
        _cfg.cmdSecret[64] = '\0';
    }

    void load() {
        memset(&_cfg, 0, sizeof(_cfg));
        _prefs.begin(NS, true);
        _cfg.configured = _prefs.getBool("cfgd", false);

        strlcpy(_cfg.wifiSsid,     _prefs.getString("ssid", "").c_str(),  sizeof(_cfg.wifiSsid));
        strlcpy(_cfg.wifiPassword, _prefs.getString("wpass", "").c_str(), sizeof(_cfg.wifiPassword));
        strlcpy(_cfg.mqttHost,     _prefs.getString("mhost", "").c_str(), sizeof(_cfg.mqttHost));
        _cfg.mqttPort = _prefs.getUShort("mport", 8883);
        strlcpy(_cfg.mqttUser,     _prefs.getString("muser", "").c_str(), sizeof(_cfg.mqttUser));
        strlcpy(_cfg.mqttPassword, _prefs.getString("mpass", "").c_str(), sizeof(_cfg.mqttPassword));
        strlcpy(_cfg.deviceName,   _prefs.getString("dname", "nexentry").c_str(), sizeof(_cfg.deviceName));
        strlcpy(_cfg.otaUser,      _prefs.getString("ouser", "admin").c_str(), sizeof(_cfg.otaUser));
        strlcpy(_cfg.otaPassword,  _prefs.getString("opass", "").c_str(), sizeof(_cfg.otaPassword));
        strlcpy(_cfg.cmdSecret,    _prefs.getString("secret", "").c_str(), sizeof(_cfg.cmdSecret));
        _prefs.end();

        if (_cfg.cmdSecret[0] == '\0') {
            _generateSecretIfEmpty();
            // Persist the freshly generated secret right away.
            _prefs.begin(NS, false);
            _prefs.putString("secret", _cfg.cmdSecret);
            _prefs.end();
        }

        _loaded = true;
        Serial.printf("[CONFIG] Loaded — configured=%d device=%s\n", _cfg.configured, _cfg.deviceName);
    }

    void save() {
        _prefs.begin(NS, false);
        _prefs.putBool("cfgd",   _cfg.configured);
        _prefs.putString("ssid",  _cfg.wifiSsid);
        _prefs.putString("wpass", _cfg.wifiPassword);
        _prefs.putString("mhost", _cfg.mqttHost);
        _prefs.putUShort("mport", _cfg.mqttPort);
        _prefs.putString("muser", _cfg.mqttUser);
        _prefs.putString("mpass", _cfg.mqttPassword);
        _prefs.putString("dname", _cfg.deviceName);
        _prefs.putString("ouser", _cfg.otaUser);
        _prefs.putString("opass", _cfg.otaPassword);
        _prefs.putString("secret", _cfg.cmdSecret);
        _prefs.end();
        Serial.println("[CONFIG] Saved to NVS");
    }

    bool isConfigured() {
        if (!_loaded) load();
        return _cfg.configured;
    }

    const DeviceConfig& get() {
        if (!_loaded) load();
        return _cfg;
    }

    void applyProvisioning(const DeviceConfig& incoming) {
        _cfg = incoming;
        _cfg.configured = true;
        if (_cfg.cmdSecret[0] == '\0') _generateSecretIfEmpty();
        save();
    }

    void setDeviceName(const char* name) {
        strlcpy(_cfg.deviceName, name, sizeof(_cfg.deviceName));
        save();
    }

    void setOtaCredentials(const char* user, const char* pass) {
        strlcpy(_cfg.otaUser, user, sizeof(_cfg.otaUser));
        strlcpy(_cfg.otaPassword, pass, sizeof(_cfg.otaPassword));
        save();
    }

    void factoryReset() {
        _prefs.begin(NS, false);
        _prefs.clear();
        _prefs.end();
        // Card registry / presence NVS namespace (access_ctrl) intentionally
        // left untouched by factory reset — this resets NETWORK/AUTH config
        // only, matching "Provisioning portal should automatically open"
        // rather than "delete attendance data". Call RFID/Presence factory
        // wipe separately if a full wipe is desired.
        Serial.println("[CONFIG] Factory reset — config namespace cleared");
    }
}
