# NexEntry v2 — Architecture

## Goals
Same external behaviour as v1 (RFID access, attendance/presence, door
control, LCD/LED/buzzer feedback, MQTT topics, dashboard), rebuilt on
FreeRTOS with secure provisioning, Secure HTTP OTA, and deployment-grade
reliability.

## Layering

```
firmware.ino          boot sequence: watchdog, queues/event group, config
                       load, provisioning gate, task creation

config/                ConfigManager (NVS-backed secrets), TLS CA cert
security/              auth.*, command_validator.*, ota_security.*
services/              mqtt_handler.* (protocol), presence.*, time_manager.*
drivers/                door.*, display.*, feedback.*, rfid_handler.*
tasks/                  one FreeRTOS task per subsystem + tasks_common.h
                        (shared queues / event-group bits / message structs)
```

`drivers/` and `services/` are the same logic as v1 (byte-for-byte where
possible), just called from a task instead of the Arduino `loop()`. `tasks/`
is the new layer that owns FreeRTOS primitives and wires everything
together with queues instead of direct function calls, per the brief.

## Task map

| Task              | Core | Priority | Type    | Responsibility |
|-------------------|------|----------|---------|----------------|
| task_rfid         | 0    | 4        | static  | Poll RC522, debounce, run presence logic, fan out to display/feedback/door/mqtt queues |
| task_door         | 0    | 3        | static  | Own the servo; consume door commands; auto-relock; held-open detection |
| task_display      | 0    | 2        | static  | Own the LCD; consume display queue; idle clock tick |
| task_feedback     | 0    | 2        | static  | Own LEDs/buzzer; consume feedback queue; held-open blink pattern |
| task_wifi         | 1    | 3        | static  | Connect/monitor WiFi; NTP kickoff; >2min-down recovery trigger |
| task_mqtt         | 1    | 3        | static  | Own MQTT connection + reconnect backoff; drain outbound publish queue; >10min-down recovery trigger |
| task_status       | 1    | 1        | static  | Periodic status publish; heap monitoring |
| task_ota          | 1    | 2        | dynamic | Secure HTTP OTA download+install; created on `START_HTTP_OTA`; self-deletes |
| task_provisioning | 1    | 2        | dynamic | WiFiManager captive portal; created on any recovery trigger; self-deletes |

Static tasks are created once in `setup()` and run forever. Dynamic tasks
(`task_ota`, `task_provisioning`) are created on demand and delete
themselves (`vTaskDelete(NULL)`) when their session ends — matching the
brief's "OTA task self-destructs after timeout" / "Delete OTA task"
requirement, and the same pattern is reused for provisioning.

## Communication

Everything that used to be a direct function call between modules (e.g.
`RFID` → `Display::welcome()` → `Feedback::granted()` → `Door::unlock()` →
`MQTT::publishTap()` in v1's `handleTap()`) is now a FreeRTOS queue send
from `task_rfid`, consumed independently by `task_display`, `task_feedback`,
`task_door`, and `task_mqtt`. See `tasks/tasks_common.h` for the message
struct definitions.

Two exceptions, by design:

- **RFID registry / presence state** (add/edit/delete card, presence reset)
  are mutated from both `task_rfid` (scans) and `task_mqtt` (admin
  commands). Round-tripping these through a queue back to `task_rfid` would
  add complexity for no real benefit — they're infrequent, non-hot-path
  operations — so `drivers/rfid_handler.cpp` and `services/presence.cpp`
  each own a FreeRTOS mutex instead and are safe to call directly from
  either task.
- **Outbound queue sends use a short timeout** (`sendWithTimeout()`, 50ms)
  rather than blocking forever. If `task_mqtt` is stalled (e.g. broker
  unreachable), a full `qMqttOut` simply drops the oldest-attempted message
  rather than stalling `task_rfid` — this is what guarantees "MQTT failure
  must never affect RFID operation" (Req. #8). Attendance state is already
  durably written to NVS by `Presence::saveState()` before the MQTT publish
  is even attempted, so nothing is lost, only the live dashboard update.

## Event group (`gSystemEvents`)

A single `EventGroupHandle_t` tracks system-wide status bits (`WIFI
_CONNECTED`, `MQTT_CONNECTED`, `TIME_SYNCED`, `OTA_ACTIVE`,
`PROVISIONING`). Tasks read/set/clear these instead of polling each other's
internal state.

## What stayed identical

- MQTT topic names and JSON payload shapes (Req. #7/#12).
- RFID scan → debounce → lookup → grant/deny → attendance toggle logic.
- LCD screens, LED/buzzer patterns, door unlock/auto-relock/held-open
  timings.
- NVS storage layout for the card registry and presence state
  (`NVS_NAMESPACE`, `NVS_KEY_REGISTRY`, `NVS_KEY_PRESENCE` unchanged) — a
  v1 device's card database loads unmodified under v2.
