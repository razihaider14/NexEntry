# NexEntry v2 — Dependencies

## Board package
- **esp32 by Espressif Systems** (Arduino-ESP32 core). v2 was written
  against the core-2.x API surface for `esp_task_wdt`; see the note in
  `firmware.ino` if you're on core 3.x.

## New libraries (not in v1)
| Library | Purpose | Install via |
|---|---|---|
| **WiFiManager** (tzapu/WiFiManager) | Captive-portal provisioning (Req. #5) | Library Manager |
| `HTTPClient` | Built into the esp32 core — downloads firmware.bin over HTTPS (Secure HTTP OTA) | (built-in) |
| `Update` | Built into the esp32 core — flash write during OTA | (built-in) |
| `mbedtls/md.h`, `mbedtls/sha256.h` | Built into the esp32 core — HMAC-SHA256 command auth, SHA-256 firmware hash | (built-in) |
| `esp_task_wdt.h` | Built into the esp32 core — per-task watchdog | (built-in) |
| `Preferences.h` | Already used in v1 for card/presence NVS; now also used by `ConfigManager` | (built-in) |

## Unchanged from v1
| Library | Purpose |
|---|---|
| `PubSubClient` | MQTT client |
| `ArduinoJson` (v6, `StaticJsonDocument`) | JSON (de)serialization |
| `WiFiClientSecure` | TLS transport for MQTT |
| `MFRC522` | RC522 RFID reader |
| `ESP32Servo` | Door lock servo |
| `LiquidCrystal_I2C` | 16x2 LCD |

## Removed
| Library | Reason |
|---|---|
| `ArduinoOTA` | Replaced by Secure HTTP OTA |
| `WebServer` (browser WebOTA, v2.0 only) | Replaced by Secure HTTP OTA — see docs/09-HTTP-OTA.md |

## FreeRTOS
No separate install — `freertos/FreeRTOS.h`, `freertos/queue.h`,
`freertos/event_groups.h`, `freertos/semphr.h`, `freertos/task.h` all ship
with the esp32 core.
