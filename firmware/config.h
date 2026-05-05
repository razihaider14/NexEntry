// ============================================================
//  config.h  —  RFID Access Control & Presence Management
//  All pins, constants, structs, and registry definition
// ============================================================

#pragma once

#include <Arduino.h>

// ── WiFi ────────────────────────────────────────────────────
#define WIFI_SSID       "your_ssid"
#define WIFI_PASSWORD   "your_password"

// ── MQTT ────────────────────────────────────────────────────
#define MQTT_BROKER     "your_pi_ip"
#define MQTT_USER       "your_mqtt_user"
#define MQTT_PASSWORD   "your_mqtt_password"
#define MQTT_PORT          8883

// CA certificate for TLS — paste your ca.crt contents here
static const char CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
your_ca_cert_here
-----END CERTIFICATE-----
)EOF";

// ── MQTT Topics ─────────────────────────────────────────────
#define TOPIC_TAP                "access/tap"
#define TOPIC_DOOR               "access/door"
#define TOPIC_ALERT              "access/alert"
#define TOPIC_STATUS             "access/status"
#define TOPIC_ENROLL_SCANNED     "access/enroll/scanned"

#define TOPIC_CMD_ENROLL         "access/cmd/enroll"
#define TOPIC_CMD_ENROLL_SAVE    "access/cmd/enroll/save"
#define TOPIC_CMD_CARD_EDIT      "access/cmd/card/edit"
#define TOPIC_CMD_CARD_DELETE    "access/cmd/card/delete"
#define TOPIC_CMD_TIME           "access/cmd/time"
#define TOPIC_CMD_DOOR           "access/cmd/door"
#define TOPIC_CMD_PRESENCE_RESET "access/cmd/presence/reset"

// ── NTP ─────────────────────────────────────────────────────
#define NTP_SERVER         "pool.ntp.org"
#define NTP_GMT_OFFSET     18000        // UTC+5 (Pakistan Standard Time)
#define NTP_DST_OFFSET     0

// ── Pins ─────────────────────────────────────────────────────
#define PIN_RC522_SS       5
#define PIN_RC522_RST      4
#define PIN_LCD_SDA        21
#define PIN_LCD_SCL        22
#define PIN_LED_GREEN      26
#define PIN_LED_RED        25
#define PIN_BUZZER         27
#define PIN_DOOR           13

// ── I2C LCD ──────────────────────────────────────────────────
#define LCD_ADDRESS        0x27
#define LCD_COLS           16
#define LCD_ROWS           2

// ── Door ─────────────────────────────────────────────────────
#define DOOR_UNLOCK_MS     3000         // how long door stays unlocked on granted tap
#define DOOR_HELD_OPEN_MS  10000        // alert if door stays open longer than this
#define SERVO_LOCKED_ANGLE    0
#define SERVO_UNLOCKED_ANGLE  90

// ── Timing ───────────────────────────────────────────────────
#define STATUS_INTERVAL_MS 30000        // heartbeat publish interval
#define DEBOUNCE_MS        2000         // minimum gap between two taps on same card

// ── Registry ─────────────────────────────────────────────────
#define MAX_CARDS          15           // max enrollable cards
#define NVS_NAMESPACE      "access_ctrl"
#define NVS_KEY_REGISTRY   "card_reg"
#define NVS_KEY_PRESENCE   "presence"

// ── Late Arrival ─────────────────────────────────────────────
#define LATE_HOUR          9            // arrivals after 09:00 are marked late
#define LATE_MINUTE        0

// ── Structs ──────────────────────────────────────────────────

struct CardRecord {
    char     uid[12];        // e.g. "A3F201CC"
    char     name[24];       // e.g. "Razi Haider"
    bool     whitelisted;    // false = blacklisted
    uint32_t tempExpiry;     // 0 = permanent, else Unix timestamp cutoff
};

struct PersonState {
    bool     isInside;
    uint32_t checkInTime;    // Unix timestamp of last CHECK_IN, 0 if outside
};

struct AccessResult {
    int      cardIndex;      // index in registry, -1 if unknown
    char     uid[12];
    char     name[24];
    char     action[12];     // "CHECK_IN" | "CHECK_OUT"
    char     access[20];     // "GRANTED" | "DENIED_BLACKLIST" | "DENIED_EXPIRED"
    uint32_t timestamp;
    bool     isLate;         // true if CHECK_IN after LATE_HOUR:LATE_MINUTE
};