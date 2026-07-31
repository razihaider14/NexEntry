#pragma once
// ---------------------------------------------------------------------------
// config.h — static, non-secret configuration.
//
// NOTE (v2): All secrets (WiFi SSID/password, MQTT host/user/password, OTA
// credentials, device name) have been REMOVED from source and now live in
// NVS, managed by ConfigManager (see config/config_manager.h). They are
// entered once via the WiFiManager captive portal on first boot.
// ---------------------------------------------------------------------------

#include <Arduino.h>

// ── MQTT topics (UNCHANGED from v1 — dashboard compatibility) ─────────────
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

// ── New (v2) administrative topics — all authenticated (see security/) ────
#define TOPIC_CMD_OTA_ENABLE     "access/cmd/ota/enable"
#define TOPIC_CMD_FACTORY_RESET  "access/cmd/factory_reset"
#define TOPIC_CMD_PROVISION_OPEN "access/cmd/provision/open"
#define TOPIC_EVT_OTA            "access/ota/status"
#define TOPIC_EVT_SECURITY       "access/security/event"

#define NTP_SERVER         "pool.ntp.org"
#define NTP_GMT_OFFSET     18000
#define NTP_DST_OFFSET     0

#define PIN_RC522_SS       5
#define PIN_RC522_RST      4
#define PIN_LCD_SDA        21
#define PIN_LCD_SCL        22
#define PIN_LED_GREEN      26
#define PIN_LED_RED        25
#define PIN_BUZZER         27
#define PIN_DOOR           13

// Hold this pin LOW at boot (or wire a button) for N resets to trigger
// "triple reset" recovery-mode provisioning (see task_provisioning).
#define PIN_RESET_BUTTON   14

#define LCD_ADDRESS        0x27
#define LCD_COLS           16
#define LCD_ROWS           2

#define DOOR_UNLOCK_MS     3000
#define DOOR_HELD_OPEN_MS  10000
#define SERVO_LOCKED_ANGLE    0
#define SERVO_UNLOCKED_ANGLE  90

#define STATUS_INTERVAL_MS 30000
#define DEBOUNCE_MS        2000

#define MAX_CARDS          15
#define NVS_NAMESPACE      "access_ctrl"
#define NVS_KEY_REGISTRY   "card_reg"
#define NVS_KEY_PRESENCE   "presence"

#define LATE_HOUR          9
#define LATE_MINUTE        0

// ── Recovery-mode thresholds (Req. #6) ─────────────────────────────────────
#define WIFI_DOWN_PROVISION_MS   (2UL * 60UL * 1000UL)   // 2 minutes
#define MQTT_DOWN_PROVISION_MS   (10UL * 60UL * 1000UL)  // 10 minutes
#define PROVISION_PORTAL_MS      (5UL * 60UL * 1000UL)   // 5 minutes
#define TRIPLE_RESET_WINDOW_MS   (10UL * 1000UL)         // 10s between resets
#define TRIPLE_RESET_COUNT       3

// ── Secure HTTP OTA ──────────────────────────────────────────────────────────
// WebOTA (browser upload) has been removed entirely. The device now pulls
// firmware.bin from a backend URL over HTTPS in response to an authenticated
// MQTT command. See tasks/task_ota.* and docs/09-HTTP-OTA.md.
#define FIRMWARE_VERSION         "2.1.0"    // bump this with every release
#define OTA_URL_MAXLEN           192
#define OTA_VERSION_MAXLEN       16
#define OTA_SHA256_HEXLEN        65         // 64 hex chars + NUL
#define OTA_SIGNATURE_MAXLEN     257        // up to a 256-hex-char (128-byte) sig + NUL
#define OTA_HTTP_CONNECT_TIMEOUT_MS  10000
#define OTA_HTTP_TOTAL_TIMEOUT_MS    (5UL * 60UL * 1000UL)  // whole download, 5 min ceiling
#define OTA_PROGRESS_REPORT_STEP_PCT 10     // publish progress every 10%

// ── Admin command auth (Req. #7) ────────────────────────────────────────────
#define CMD_MAX_CLOCK_SKEW_S     60      // reject ts more than 60s old/future
#define CMD_NONCE_CACHE_SIZE     32      // replay-protection ring buffer
#define CMD_RATE_LIMIT_WINDOW_MS 10000   // admin cmds only
#define CMD_RATE_LIMIT_MAX       5       // per window, per topic class

struct CardRecord {
    char     uid[12];
    char     name[24];
    bool     whitelisted;
    uint32_t tempExpiry;
};

struct PersonState {
    bool     isInside;
    uint32_t checkInTime;
};

struct AccessResult {
    int      cardIndex;
    char     uid[12];
    char     name[24];
    char     action[12];
    char     access[20];
    uint32_t timestamp;
    bool     isLate;
};
