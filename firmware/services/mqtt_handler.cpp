#include "mqtt_handler.h"
#include "../config/config_manager.h"
#include "../config/tls_cert.h"
#include "../security/auth.h"
#include "../security/command_validator.h"
#include "../drivers/rfid_handler.h"
#include "../services/presence.h"
#include "../services/time_manager.h"
#include "../tasks/tasks_common.h"
#include "../tasks/task_ota.h"
#include "../tasks/task_provisioning.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace MQTT {

    static WiFiClientSecure _wifiClient;
    static PubSubClient     _client(_wifiClient);

    static void _onMessage(char* topic, byte* payload, unsigned int length);
    static void _subscribeAll();

    // ── Admin command handlers (same behaviour as v1; now behind auth) ────
    static void _rejectUnauthorized(const char* topic, const char* reason) {
        Serial.printf("[MQTT] REJECTED %s — %s\n", topic, reason);
        publishSecurityEvent(topic, reason);
    }

    static void _handleEnroll(JsonDocument& doc, const char* topic) {
        char reason[32];
        if (!CommandValidator::isAuthorized(topic, doc, reason, sizeof(reason))) {
            _rejectUnauthorized(topic, reason); return;
        }
        const char* cmd = doc["cmd"];
        if (!cmd) return;

        if (strcmp(cmd, "START") == 0) {
            RFID::setEnrollMode(true);
            DisplayMsg dm{DisplayMsgType::ENROLL_MODE};
            sendWithTimeout(qDisplay, &dm);
            FeedbackMsg fm{FeedbackMsgType::ENROLL_READY};
            sendWithTimeout(qFeedback, &fm);
            Serial.println("[MQTT] Enrollment mode START");
        } else if (strcmp(cmd, "STOP") == 0) {
            RFID::setEnrollMode(false);
            DisplayMsg dm{DisplayMsgType::IDLE};
            sendWithTimeout(qDisplay, &dm);
            Serial.println("[MQTT] Enrollment mode STOP");
        }
    }

    static void _handleEnrollSave(JsonDocument& doc, const char* topic) {
        char reason[32];
        if (!CommandValidator::isAuthorized(topic, doc, reason, sizeof(reason))) {
            _rejectUnauthorized(topic, reason); return;
        }
        const char* uid  = doc["uid"];
        const char* name = doc["name"];
        bool     whitelisted = doc["whitelisted"] | true;
        uint32_t tempExpiry  = doc["tempExpiry"]  | 0;
        if (!uid || !name) { Serial.println("[MQTT] enrollSave — missing uid or name"); return; }

        CardRecord record{};
        strncpy(record.uid,  uid,  sizeof(record.uid)  - 1);
        strncpy(record.name, name, sizeof(record.name) - 1);
        record.whitelisted = whitelisted;
        record.tempExpiry  = tempExpiry;

        if (RFID::addCard(record)) {
            DisplayMsg dm{DisplayMsgType::ENROLL_SAVED};
            strlcpy(dm.text, name, sizeof(dm.text));
            sendWithTimeout(qDisplay, &dm);
            FeedbackMsg fm{FeedbackMsgType::ENROLL_SAVED};
            sendWithTimeout(qFeedback, &fm);
        } else {
            Serial.println("[MQTT] enrollSave — addCard failed");
        }
    }

    static void _handleCardEdit(JsonDocument& doc, const char* topic) {
        char reason[32];
        if (!CommandValidator::isAuthorized(topic, doc, reason, sizeof(reason))) {
            _rejectUnauthorized(topic, reason); return;
        }
        const char* uid  = doc["uid"];
        const char* name = doc["name"];
        bool     whitelisted = doc["whitelisted"] | true;
        uint32_t tempExpiry  = doc["tempExpiry"]  | 0;
        if (!uid || !name) { Serial.println("[MQTT] cardEdit — missing fields"); return; }
        RFID::editCard(uid, name, whitelisted, tempExpiry);
    }

    static void _handleCardDelete(JsonDocument& doc, const char* topic) {
        char reason[32];
        if (!CommandValidator::isAuthorized(topic, doc, reason, sizeof(reason))) {
            _rejectUnauthorized(topic, reason); return;
        }
        const char* uid = doc["uid"];
        if (!uid) return;
        RFID::deleteCard(uid);
    }

    // Payload (all fields optional except "mode" for enable/disable):
    //   {"mode":"DEMO"|"REAL",
    //    "offsetSeconds": N,        // legacy fast-forward, still supported
    //    "action":"PAUSE"|"RESUME"|"RESET"|"FAST_FORWARD",
    //    "datetime": <unix ts>,     // demo_datetime — jump to an absolute time
    //    "speedMultiplier": 60.0}   // demo_speed_multiplier
    //
    // Not on the protected-topics list in the brief (kept payload-
    // compatible, unauthenticated, matching v1) — see security audit for
    // the recommendation to add auth here in a future revision. `scope`
    // is accepted-but-ignored today, reserved for a future backend-driven
    // organization-wide Demo Mode (device currently only understands
    // per-device scope).
    static void _handleTimeCommand(JsonDocument& doc) {
        const char* mode = doc["mode"];

        if (mode && strcmp(mode, "DEMO") == 0) {
            bool wasEnabled = TimeManager::isDemoMode();
            if (!wasEnabled) TimeManager::setDemoMode(true);

            if (doc["datetime"].is<uint32_t>() || doc["datetime"].is<long>()) {
                TimeManager::setDemoDateTime(doc["datetime"].as<uint32_t>());
            }
            if (doc["speedMultiplier"].is<float>() || doc["speedMultiplier"].is<double>()) {
                TimeManager::setDemoSpeedMultiplier(doc["speedMultiplier"].as<float>());
            }
            int32_t offset = doc["offsetSeconds"] | 0; // legacy field, still honored
            if (offset != 0) TimeManager::fastForward(offset);

            if (!wasEnabled) {
                DisplayMsg dm{DisplayMsgType::DEMO_ON};
                sendWithTimeout(qDisplay, &dm);
                FeedbackMsg fm{FeedbackMsgType::DEMO_MODE};
                sendWithTimeout(qFeedback, &fm);
            }
        } else if (mode && strcmp(mode, "REAL") == 0) {
            TimeManager::setDemoMode(false);
            DisplayMsg dm{DisplayMsgType::DEMO_OFF};
            sendWithTimeout(qDisplay, &dm);
            FeedbackMsg fm{FeedbackMsgType::DEMO_MODE};
            sendWithTimeout(qFeedback, &fm);
        }

        // Demo transport controls — independent of `mode` so a dashboard
        // can e.g. pause without also re-sending "mode":"DEMO".
        const char* action = doc["action"];
        if (action) {
            if      (strcmp(action, "PAUSE") == 0)  TimeManager::pauseDemo();
            else if (strcmp(action, "RESUME") == 0) TimeManager::resumeDemo();
            else if (strcmp(action, "RESET") == 0)  TimeManager::resetDemo();
            else if (strcmp(action, "FAST_FORWARD") == 0) {
                int32_t offset = doc["offsetSeconds"] | 0;
                TimeManager::fastForward(offset);
            }
        }
    }

    static void _handleDoorCommand(JsonDocument& doc, const char* topic) {
        char reason[32];
        if (!CommandValidator::isAuthorized(topic, doc, reason, sizeof(reason))) {
            _rejectUnauthorized(topic, reason); return;
        }
        const char* cmd = doc["cmd"];
        if (!cmd) return;

        DoorCmdMsg dc{};
        if (strcmp(cmd, "UNLOCK") == 0) { dc.type = DoorCmdType::UNLOCK; dc.durationMs = DOOR_UNLOCK_MS; }
        else if (strcmp(cmd, "LOCK") == 0) { dc.type = DoorCmdType::LOCK; }
        else return;
        sendWithTimeout(qDoorCmd, &dc);
        Serial.printf("[MQTT] Remote door cmd: %s\n", cmd);
    }

    static void _handlePresenceReset(JsonDocument& doc) {
        // Not on the protected list in the spec (kept payload-compatible,
        // unauthenticated, matching v1).
        const char* action = doc["action"];
        if (!action) return;
        if (strcmp(action, "RESET_ALL") == 0) {
            Presence::resetAll();
            Serial.println("[MQTT] Presence reset — all OUT");
        }
    }

    static void _handleOtaEnable(JsonDocument& doc, const char* topic) {
        char reason[32];
        if (!CommandValidator::isAuthorized(topic, doc, reason, sizeof(reason))) {
            _rejectUnauthorized(topic, reason); return;
        }
        const char* cmd = doc["cmd"] | "";
        if (strcmp(cmd, "START_HTTP_OTA") != 0) {
            Serial.printf("[MQTT] Unknown OTA cmd: %s\n", cmd);
            return;
        }

        HttpOtaJob job{};
        strlcpy(job.version,   doc["version"]   | "", sizeof(job.version));
        strlcpy(job.url,       doc["url"]       | "", sizeof(job.url));
        strlcpy(job.sha256,    doc["sha256"]    | "", sizeof(job.sha256));
        strlcpy(job.signature, doc["signature"] | "", sizeof(job.signature));
        job.force = doc["force"] | false;

        Serial.printf("[MQTT] START_HTTP_OTA — version=%s url=%s\n", job.version, job.url);
        requestStartHttpOta(job);
    }

    static void _handleFactoryReset(JsonDocument& doc, const char* topic) {
        char reason[32];
        if (!CommandValidator::isAuthorized(topic, doc, reason, sizeof(reason))) {
            _rejectUnauthorized(topic, reason); return;
        }
        Serial.println("[MQTT] FACTORY_RESET requested — clearing config, rebooting");
        publishSecurityEvent(topic, "factory reset executed");
        delay(200); // let the publish flush
        ConfigManager::factoryReset();
        ESP.restart();
    }

    static void _handleProvisionOpen(JsonDocument& doc, const char* topic) {
        char reason[32];
        if (!CommandValidator::isAuthorized(topic, doc, reason, sizeof(reason))) {
            _rejectUnauthorized(topic, reason); return;
        }
        Serial.println("[MQTT] Remote request to open provisioning portal");
        requestOpenProvisioningPortal();
    }

    static void _onMessage(char* topic, byte* payload, unsigned int length) {
        char msg[512];
        length = min(length, (unsigned int)511);
        memcpy(msg, payload, length);
        msg[length] = '\0';

        Serial.printf("[MQTT] Received [%s]: %s\n", topic, msg);

        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, msg);
        if (err) { Serial.printf("[MQTT] JSON parse error: %s\n", err.c_str()); return; }

        if      (strcmp(topic, TOPIC_CMD_ENROLL)         == 0) _handleEnroll(doc, topic);
        else if (strcmp(topic, TOPIC_CMD_ENROLL_SAVE)    == 0) _handleEnrollSave(doc, topic);
        else if (strcmp(topic, TOPIC_CMD_CARD_EDIT)      == 0) _handleCardEdit(doc, topic);
        else if (strcmp(topic, TOPIC_CMD_CARD_DELETE)    == 0) _handleCardDelete(doc, topic);
        else if (strcmp(topic, TOPIC_CMD_TIME)           == 0) _handleTimeCommand(doc);
        else if (strcmp(topic, TOPIC_CMD_DOOR)           == 0) _handleDoorCommand(doc, topic);
        else if (strcmp(topic, TOPIC_CMD_PRESENCE_RESET) == 0) _handlePresenceReset(doc);
        else if (strcmp(topic, TOPIC_CMD_OTA_ENABLE)     == 0) _handleOtaEnable(doc, topic);
        else if (strcmp(topic, TOPIC_CMD_FACTORY_RESET)  == 0) _handleFactoryReset(doc, topic);
        else if (strcmp(topic, TOPIC_CMD_PROVISION_OPEN) == 0) _handleProvisionOpen(doc, topic);
    }

    static void _subscribeAll() {
        _client.subscribe(TOPIC_CMD_ENROLL);
        _client.subscribe(TOPIC_CMD_ENROLL_SAVE);
        _client.subscribe(TOPIC_CMD_CARD_EDIT);
        _client.subscribe(TOPIC_CMD_CARD_DELETE);
        _client.subscribe(TOPIC_CMD_TIME);
        _client.subscribe(TOPIC_CMD_DOOR);
        _client.subscribe(TOPIC_CMD_PRESENCE_RESET);
        _client.subscribe(TOPIC_CMD_OTA_ENABLE);
        _client.subscribe(TOPIC_CMD_FACTORY_RESET);
        _client.subscribe(TOPIC_CMD_PROVISION_OPEN);
        Serial.println("[MQTT] Subscribed to all command topics");
    }

    void init() {
        const DeviceConfig& cfg = ConfigManager::get();
        Auth::init(cfg.cmdSecret);

        _wifiClient.setCACert(CA_CERT);
        _client.setServer(cfg.mqttHost, cfg.mqttPort);
        _client.setCallback(_onMessage);
        _client.setBufferSize(512);
        Serial.println("[MQTT] Init OK (connection handled by task_mqtt)");
    }

    void loop() {
        // Non-blocking single connection attempt — task_mqtt owns backoff
        // timing so a down broker never blocks RFID/attendance (Req. #8).
        if (!_client.connected()) {
            const DeviceConfig& cfg = ConfigManager::get();
            char clientId[48];
            snprintf(clientId, sizeof(clientId), "%s-%08X", cfg.deviceName, (uint32_t)ESP.getEfuseMac());

            if (_client.connect(clientId, cfg.mqttUser, cfg.mqttPassword)) {
                Serial.println("[MQTT] Connected");
                _subscribeAll();
                publishStatus();
                xEventGroupSetBits(gSystemEvents, BIT_MQTT_CONNECTED);
            } else {
                xEventGroupClearBits(gSystemEvents, BIT_MQTT_CONNECTED);
            }
            return;
        }
        xEventGroupSetBits(gSystemEvents, BIT_MQTT_CONNECTED);
        _client.loop();
    }

    bool connected() { return _client.connected(); }

    void publishTap(const AccessResult& result) {
        StaticJsonDocument<256> doc;
        doc["uid"] = result.uid;
        doc["name"] = result.name;
        doc["action"] = result.action;
        doc["access"] = result.access;
        doc["timestamp"] = result.timestamp;
        doc["isLate"] = result.isLate;
        doc["demo"] = TimeManager::isDemoMode();
        if (strcmp(result.action, "CHECK_OUT") == 0 && result.cardIndex >= 0) {
            doc["checkInTime"] = Presence::getCheckInTime(result.cardIndex);
        }
        char buf[256];
        serializeJson(doc, buf);
        _client.publish(TOPIC_TAP, buf);
    }

    void publishAlert(const char* type, const char* uid) {
        StaticJsonDocument<128> doc;
        doc["type"] = type;
        doc["uid"] = uid;
        doc["timestamp"] = TimeManager::now();
        doc["demo"] = TimeManager::isDemoMode();
        char buf[128];
        serializeJson(doc, buf);
        _client.publish(TOPIC_ALERT, buf);
    }

    void publishDoorEvent(const char* event) {
        StaticJsonDocument<128> doc;
        doc["event"] = event;
        doc["timestamp"] = TimeManager::now();
        doc["demo"] = TimeManager::isDemoMode();
        char buf[128];
        serializeJson(doc, buf);
        _client.publish(TOPIC_DOOR, buf);
    }

    void publishStatus() {
        StaticJsonDocument<256> doc;
        doc["uptime"] = millis() / 1000;
        doc["rssi"] = WiFi.RSSI();
        char t[10]; TimeManager::formattedTime(t, sizeof(t));
        doc["time"] = t;
        bool demo = TimeManager::isDemoMode();
        doc["demo"] = demo; // "demo_enabled" — device always knows Production vs Demo
        if (demo) {
            doc["demoPaused"] = TimeManager::isDemoPaused();
            doc["demoSpeed"]  = TimeManager::getDemoSpeedMultiplier();
        }
        doc["cards"] = RFID::getCardCount();
        doc["heapFree"] = ESP.getFreeHeap();
        doc["heapMinFree"] = ESP.getMinFreeHeap();
        doc["fwVersion"] = FIRMWARE_VERSION;
        char buf[256];
        serializeJson(doc, buf);
        _client.publish(TOPIC_STATUS, buf);
    }

    void publishEnrollScanned(const char* uid) {
        StaticJsonDocument<64> doc;
        doc["uid"] = uid;
        char buf[64];
        serializeJson(doc, buf);
        _client.publish(TOPIC_ENROLL_SCANNED, buf);
    }

    void publishOtaStatus(const char* status, int16_t progress) {
        StaticJsonDocument<128> doc;
        doc["status"] = status;
        if (progress >= 0) doc["progress"] = progress;
        doc["timestamp"] = TimeManager::now();
        char buf[128];
        serializeJson(doc, buf);
        _client.publish(TOPIC_EVT_OTA, buf);
    }

    void publishSecurityEvent(const char* topic, const char* reason) {
        StaticJsonDocument<160> doc;
        doc["topic"] = topic;
        doc["reason"] = reason;
        doc["timestamp"] = TimeManager::now();
        char buf[160];
        serializeJson(doc, buf);
        _client.publish(TOPIC_EVT_SECURITY, buf);
    }

    void processOutboundMessage(const MqttPublishMsg& msg) {
        switch (msg.type) {
            case MqttPublishType::TAP:            publishTap(msg.access); break;
            case MqttPublishType::ALERT:          publishAlert(msg.alertType, msg.alertUid); break;
            case MqttPublishType::DOOR_EVENT:     publishDoorEvent(msg.doorEvent); break;
            case MqttPublishType::STATUS:         publishStatus(); break;
            case MqttPublishType::ENROLL_SCANNED: publishEnrollScanned(msg.text); break;
            case MqttPublishType::OTA_STATUS:     publishOtaStatus(msg.text, msg.otaProgress); break;
            case MqttPublishType::SECURITY_EVENT: publishSecurityEvent(msg.alertType, msg.text); break;
        }
    }
}
