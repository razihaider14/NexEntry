#include "mqtt_handler.h"
#include "config.h"
#include "rfid_handler.h"
#include "presence.h"
#include "time_manager.h"
#include "door.h"
#include "feedback.h"
#include "display.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace MQTT {

    static WiFiClientSecure _wifiClient;
    static PubSubClient     _client(_wifiClient);
    static void (*_onConnectCallback)() = nullptr;

    static void _onMessage(char* topic, byte* payload, unsigned int length);
    static void _reconnect();
    static void _subscribeAll();

    static void _handleEnroll(const JsonDocument& doc) {
        const char* cmd = doc["cmd"];
        if (!cmd) return;

        if (strcmp(cmd, "START") == 0) {
            RFID::setEnrollMode(true);
            Display::enrollMode();
            Feedback::enrollReady();
            Serial.println("[MQTT] Enrollment mode START");
        } else if (strcmp(cmd, "STOP") == 0) {
            RFID::setEnrollMode(false);
            Display::idle();
            Serial.println("[MQTT] Enrollment mode STOP");
        }
    }

    static void _handleEnrollSave(const JsonDocument& doc) {
        const char* uid        = doc["uid"];
        const char* name       = doc["name"];
        bool        whitelisted = doc["whitelisted"] | true;
        uint32_t    tempExpiry  = doc["tempExpiry"]  | 0;

        if (!uid || !name) {
            Serial.println("[MQTT] enrollSave — missing uid or name");
            return;
        }

        CardRecord record;
        strncpy(record.uid,  uid,  sizeof(record.uid)  - 1);
        strncpy(record.name, name, sizeof(record.name) - 1);
        record.uid[sizeof(record.uid)   - 1] = '\0';
        record.name[sizeof(record.name) - 1] = '\0';
        record.whitelisted = whitelisted;
        record.tempExpiry  = tempExpiry;

        if (RFID::addCard(record)) {
            Display::enrollSaved(name);
            Feedback::enrollSaved();
        } else {
            Serial.println("[MQTT] enrollSave — addCard failed");
        }
    }

    static void _handleCardEdit(const JsonDocument& doc) {
        const char* uid        = doc["uid"];
        const char* name       = doc["name"];
        bool        whitelisted = doc["whitelisted"] | true;
        uint32_t    tempExpiry  = doc["tempExpiry"]  | 0;

        if (!uid || !name) {
            Serial.println("[MQTT] cardEdit — missing fields");
            return;
        }

        RFID::editCard(String(uid), name, whitelisted, tempExpiry);
    }

    static void _handleCardDelete(const JsonDocument& doc) {
        const char* uid = doc["uid"];
        if (!uid) return;
        RFID::deleteCard(String(uid));
    }

    static void _handleTimeCommand(const JsonDocument& doc) {
        const char* mode = doc["mode"];
        if (!mode) return;

        if (strcmp(mode, "DEMO") == 0) {
            int32_t offset = doc["offsetSeconds"] | 0;
            if (!TimeManager::isDemoMode()) {
                TimeManager::setDemoMode(true);
                Display::demoModeOn();
                Feedback::demoMode();
            }
            TimeManager::addDemoOffset(offset);
            Serial.printf("[MQTT] Demo offset applied: %ds\n", offset);
        } else if (strcmp(mode, "REAL") == 0) {
            TimeManager::setDemoMode(false);
            Display::demoModeOff();
            Feedback::demoMode();
            Serial.println("[MQTT] Switched to real time");
        }
    }

    static void _handleDoorCommand(const JsonDocument& doc) {
        const char* cmd = doc["cmd"];
        if (!cmd) return;

        if (strcmp(cmd, "UNLOCK") == 0) {
            Door::unlock(DOOR_UNLOCK_MS);
            publishDoorEvent("UNLOCKED");
            Serial.println("[MQTT] Remote unlock triggered");
        } else if (strcmp(cmd, "LOCK") == 0) {
            Door::lock();
            publishDoorEvent("LOCKED");
            Serial.println("[MQTT] Remote lock triggered");
        }
    }

    static void _handlePresenceReset(const JsonDocument& doc) {
        const char* action = doc["action"];
        if (!action) return;

        if (strcmp(action, "RESET_ALL") == 0) {
            Presence::resetAll();
            Serial.println("[MQTT] Presence reset — all OUT");
        }
    }

    static void _onMessage(char* topic, byte* payload, unsigned int length) {
        char msg[512];
        length = min(length, (unsigned int)511);
        memcpy(msg, payload, length);
        msg[length] = '\0';

        Serial.printf("[MQTT] Received [%s]: %s\n", topic, msg);

        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, msg);
        if (err) {
            Serial.printf("[MQTT] JSON parse error: %s\n", err.c_str());
            return;
        }

        if (strcmp(topic, TOPIC_CMD_ENROLL)      == 0) _handleEnroll(doc);
        else if (strcmp(topic, TOPIC_CMD_ENROLL_SAVE)  == 0) _handleEnrollSave(doc);
        else if (strcmp(topic, TOPIC_CMD_CARD_EDIT)    == 0) _handleCardEdit(doc);
        else if (strcmp(topic, TOPIC_CMD_CARD_DELETE)  == 0) _handleCardDelete(doc);
        else if (strcmp(topic, TOPIC_CMD_TIME)         == 0) _handleTimeCommand(doc);
        else if (strcmp(topic, TOPIC_CMD_DOOR)         == 0) _handleDoorCommand(doc);
        else if (strcmp(topic, TOPIC_CMD_PRESENCE_RESET) == 0) _handlePresenceReset(doc);
    }

    static void _subscribeAll() {
        _client.subscribe(TOPIC_CMD_ENROLL);
        _client.subscribe(TOPIC_CMD_ENROLL_SAVE);
        _client.subscribe(TOPIC_CMD_CARD_EDIT);
        _client.subscribe(TOPIC_CMD_CARD_DELETE);
        _client.subscribe(TOPIC_CMD_TIME);
        _client.subscribe(TOPIC_CMD_DOOR);
        _client.subscribe(TOPIC_CMD_PRESENCE_RESET);
        Serial.println("[MQTT] Subscribed to all command topics");
    }

    static void _reconnect() {
        while (!_client.connected()) {
            Display::mqttReconnecting();
            Serial.print("[MQTT] Connecting...");

            String clientId = "ESP32-Access-" + String((uint32_t)ESP.getEfuseMac(), HEX);

            if (_client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
                Serial.println(" OK");
                _subscribeAll();
                if (_onConnectCallback) _onConnectCallback();
            } else {
                Serial.printf(" failed (state=%d) — retry in 5s\n", _client.state());
                delay(5000);
            }
        }
    }

    void init(void (*onConnectCallback)()) {
        _onConnectCallback = onConnectCallback;
        _wifiClient.setCACert(CA_CERT);
        _client.setServer(MQTT_BROKER, MQTT_PORT);
        _client.setCallback(_onMessage);
        _client.setBufferSize(512);
        _reconnect();
        Serial.println("[MQTT] Init OK");
    }

    void loop() {
        if (!_client.connected()) _reconnect();
        _client.loop();
    }

    bool connected() {
        return _client.connected();
    }

    void publishTap(const AccessResult& result) {
        StaticJsonDocument<256> doc;
        doc["uid"]       = result.uid;
        doc["name"]      = result.name;
        doc["action"]    = result.action;
        doc["access"]    = result.access;
        doc["timestamp"] = result.timestamp;
        doc["isLate"]    = result.isLate;
        doc["demo"]      = TimeManager::isDemoMode();

        if (strcmp(result.action, "CHECK_OUT") == 0) {
            int idx = result.cardIndex;
            if (idx >= 0) {
                doc["checkInTime"] = Presence::getCheckInTime(idx);
            }
        }

        char buf[256];
        serializeJson(doc, buf);
        _client.publish(TOPIC_TAP, buf);
        Serial.printf("[MQTT] Published tap: %s\n", buf);
    }

    void publishAlert(const char* type, const char* uid) {
        StaticJsonDocument<128> doc;
        doc["type"]      = type;
        doc["uid"]       = uid;
        doc["timestamp"] = TimeManager::now();
        doc["demo"]      = TimeManager::isDemoMode();

        char buf[128];
        serializeJson(doc, buf);
        _client.publish(TOPIC_ALERT, buf);
        Serial.printf("[MQTT] Published alert: %s\n", buf);
    }

    void publishDoorEvent(const char* event) {
        StaticJsonDocument<128> doc;
        doc["event"]     = event;
        doc["timestamp"] = TimeManager::now();
        doc["demo"]      = TimeManager::isDemoMode();

        char buf[128];
        serializeJson(doc, buf);
        _client.publish(TOPIC_DOOR, buf);
    }

    void publishStatus() {
        StaticJsonDocument<128> doc;
        doc["uptime"]    = millis() / 1000;
        doc["rssi"]      = WiFi.RSSI();
        doc["time"]      = TimeManager::formattedTime();
        doc["demo"]      = TimeManager::isDemoMode();
        doc["cards"]     = RFID::getCardCount();

        char buf[128];
        serializeJson(doc, buf);
        _client.publish(TOPIC_STATUS, buf);
    }

    void publishEnrollScanned(const char* uid) {
        StaticJsonDocument<64> doc;
        doc["uid"] = uid;

        char buf[64];
        serializeJson(doc, buf);
        _client.publish(TOPIC_ENROLL_SCANNED, buf);
        Serial.printf("[MQTT] Enroll scanned: %s\n", uid);
    }
} 