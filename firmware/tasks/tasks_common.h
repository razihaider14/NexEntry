#pragma once
// ---------------------------------------------------------------------------
// tasks_common.h — shared queues, event-group bits, and message structs used
// for inter-task communication (Req. #1: "Queue-based communication should
// be preferred over tightly coupled function calls").
//
// Flow implemented:
//   RFID Task --(qDisplay, qFeedback, qMqttOut)--> Display/Feedback/MQTT Tasks
//   Door  Task --(qMqttOut)--> MQTT Task (door events)
//   MQTT  Task --(qDoorCmd)--> Door Task (remote lock/unlock)
//   MQTT  Task --(direct call, mutex-protected)--> RFID/Presence registries
//              for enroll/edit/delete/reset (infrequent, low-latency-
//              tolerant admin ops — see drivers/rfid_handler.cpp,
//              services/presence.cpp for the mutexes that make this safe).
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include "../config.h"

// ── Event group bits (system-wide status) ──────────────────────────────────
#define BIT_WIFI_CONNECTED    (1 << 0)
#define BIT_MQTT_CONNECTED    (1 << 1)
#define BIT_TIME_SYNCED       (1 << 2)
#define BIT_OTA_ACTIVE        (1 << 3)
#define BIT_PROVISIONING      (1 << 4)
#define BIT_SHOULD_PROVISION  (1 << 5)   // set by watchdog/reconnect logic

extern EventGroupHandle_t gSystemEvents;

// ── Display queue ───────────────────────────────────────────────────────────
enum class DisplayMsgType : uint8_t {
    IDLE, WELCOME, GOODBYE, DENIED, UNKNOWN_CARD, ENROLL_MODE,
    ENROLL_SCANNED, ENROLL_SAVED, DEMO_ON, DEMO_OFF, CONNECTING,
    MQTT_RECONNECTING, PROVISIONING, OTA_ACTIVE
};
struct DisplayMsg {
    DisplayMsgType type;
    char           text[24];   // name / uid / reason, depending on type
};
extern QueueHandle_t qDisplay;

// ── Feedback queue ──────────────────────────────────────────────────────────
enum class FeedbackMsgType : uint8_t {
    GRANTED, DENIED, UNKNOWN_CARD, ENROLL_READY, ENROLL_SAVED,
    DEMO_MODE, DOOR_HELD_ON, DOOR_HELD_OFF
};
struct FeedbackMsg { FeedbackMsgType type; };
extern QueueHandle_t qFeedback;

// ── Door command queue (MQTT/other tasks -> Door Task) ─────────────────────
enum class DoorCmdType : uint8_t { UNLOCK, LOCK };
struct DoorCmdMsg { DoorCmdType type; uint32_t durationMs; };
extern QueueHandle_t qDoorCmd;

// ── Outbound MQTT publish queue (any task -> MQTT Task) ─────────────────────
enum class MqttPublishType : uint8_t {
    TAP, ALERT, DOOR_EVENT, STATUS, ENROLL_SCANNED, OTA_STATUS, SECURITY_EVENT
};
struct MqttPublishMsg {
    MqttPublishType type;
    AccessResult     access;         // TAP
    char             alertType[20];  // ALERT / SECURITY_EVENT
    char             alertUid[12];   // ALERT
    char             doorEvent[16];  // DOOR_EVENT
    char             text[64];       // ENROLL_SCANNED uid / OTA_STATUS / SECURITY_EVENT reason
    int16_t          otaProgress = -1; // OTA_STATUS only; -1 = omit from payload
};
extern QueueHandle_t qMqttOut;

// ── Helper: non-blocking-ish send with a short timeout so a stalled
//    consumer (e.g. MQTT task offline) never blocks the RFID hot path. ─────
inline bool sendWithTimeout(QueueHandle_t q, const void* item, TickType_t ticks = pdMS_TO_TICKS(50)) {
    return xQueueSend(q, item, ticks) == pdTRUE;
}
