// ============================================================
//  NexEntry  —  Setup, loop, and module orchestration
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "time_manager.h"
#include "feedback.h"
#include "display.h"
#include "door.h"
#include "rfid_handler.h"
#include "presence.h"
#include "mqtt_handler.h"

// ── Timers ───────────────────────────────────────────────────
static uint32_t _lastStatusPublish  = 0;
static uint32_t _lastTimeUpdate     = 0;
static uint32_t _lastCardTime       = 0;  // debounce tracker
static String   _lastCardUID        = "";

// ── WiFi ─────────────────────────────────────────────────────

static void connectWiFi() {
    Display::connecting();
    Serial.printf("[WIFI] Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    Serial.printf(" OK — IP: %s\n", WiFi.localIP().toString().c_str());
}

// ── OTA ──────────────────────────────────────────────────────

static void initOTA() {
    ArduinoOTA.setHostname("nexentry");
    ArduinoOTA.onStart([]() {
        Serial.println("[OTA] Start");
        Display::connecting();
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("[OTA] Done");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error [%u]\n", error);
    });
    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}

// ── MQTT connect callback ─────────────────────────────────────
// Fires after every (re)connect — use it to publish initial status

static void onMQTTConnect() {
    MQTT::publishStatus();
}

// ── Tap handling ─────────────────────────────────────────────

static void handleTap(const String& uid, int cardIndex) {
    // ── Enrollment mode ──────────────────────────────────────
    if (RFID::isEnrollMode()) {
        Display::enrollScanned(uid.c_str());
        Feedback::enrollReady();
        MQTT::publishEnrollScanned(uid.c_str());
        return;
    }

    // ── Normal operation ─────────────────────────────────────
    bool granted = Presence::processTap(cardIndex, uid);
    AccessResult result = Presence::getLastResult();

    if (granted) {
        // CHECK_IN or CHECK_OUT
        if (strcmp(result.action, "CHECK_IN") == 0) {
            Display::welcome(result.name);
        } else {
            Display::goodbye(result.name);
        }
        Feedback::granted();
        Door::unlock(DOOR_UNLOCK_MS);
        MQTT::publishTap(result);
        MQTT::publishDoorEvent("UNLOCKED");

    } else {
        // Denied or unknown
        if (strcmp(result.access, "UNKNOWN") == 0) {
            Display::unknown();
            Feedback::unknown();
            MQTT::publishAlert("UNKNOWN_CARD", uid.c_str());
        } else if (strcmp(result.access, "DENIED_BLACKLIST") == 0) {
            Display::denied("Blacklisted");
            Feedback::denied();
            MQTT::publishTap(result);
        } else if (strcmp(result.access, "DENIED_EXPIRED") == 0) {
            Display::denied("Access Expired");
            Feedback::denied();
            MQTT::publishTap(result);
        }
    }

    // Return to idle after feedback settles
    // feedback functions use delay() so by the time we get here
    // the LED/beep is already done
    delay(500);
    Display::idle();
}

// ── Setup ────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== NexEntry — Booting ===");

    // Order matters — display and feedback first so
    // we can show status during the rest of init
    Display::init();
    Feedback::init();
    Door::init();

    connectWiFi();
    initOTA();
    TimeManager::begin();

    RFID::init();
    Presence::init();
    MQTT::init(onMQTTConnect);

    Display::idle();
    Serial.println("=== Boot complete ===\n");
}

// ── Loop ─────────────────────────────────────────────────────

void loop() {
    ArduinoOTA.handle();
    MQTT::loop();
    Door::tick();
    if (Door::autoRelockFired()) {
        MQTT::publishDoorEvent("LOCKED");
    }
    Feedback::tick();

    uint32_t now = millis();

    // ── Card scan ────────────────────────────────────────────
    if (RFID::cardPresent()) {
        String uid = RFID::readUID();

        // Debounce — ignore same card within DEBOUNCE_MS
        bool sameCard    = (uid == _lastCardUID);
        bool tooSoon     = (now - _lastCardTime) < DEBOUNCE_MS;

        if (!(sameCard && tooSoon)) {
            _lastCardUID  = uid;
            _lastCardTime = now;
            int cardIndex = RFID::lookupCard(uid);
            handleTap(uid, cardIndex);
        }
    }

    // ── Held-open alert ──────────────────────────────────────
    if (Door::isHeldOpen()) {
        Feedback::doorHeldOn();
        MQTT::publishAlert("DOOR_HELD_OPEN", "");
        MQTT::publishDoorEvent("HELD_OPEN");
        // isHeldOpen() is a one-shot flag — won't fire again
        // until next unlock cycle (handled in door.cpp)
    }

    // ── Clock update on LCD ──────────────────────────────────
    if (now - _lastTimeUpdate >= 1000) {
        _lastTimeUpdate = now;
        Display::updateTime(TimeManager::formatted());
    }

    // ── Heartbeat status publish ─────────────────────────────
    if (now - _lastStatusPublish >= STATUS_INTERVAL_MS) {
        _lastStatusPublish = now;
        MQTT::publishStatus();
    }
}