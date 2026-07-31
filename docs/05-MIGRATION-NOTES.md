# NexEntry v2 — Migration Notes

## Upgrading an existing v1 device

1. **Card registry / presence state survive.** They're read from the same
   NVS namespace/keys (`access_ctrl` / `card_reg` / `presence`) v1 used, so
   a v1 → v2 flash keeps every enrolled card and current in/out state.
2. **WiFi/MQTT credentials do NOT survive** — they were hardcoded in v1's
   `config.h` and never written to NVS. On first v2 boot, `ConfigManager::
   isConfigured()` is false, so the device opens the `NexEntry-Setup`
   captive portal automatically. Connect to it from a phone, fill in the
   same WiFi/MQTT details v1 had hardcoded, plus a device name and OTA
   credentials, and submit — the device reboots into normal operation.
3. **First OTA still has to happen over serial/USB.** Secure HTTP OTA
   replaces ArduinoOTA for *future* updates, but you need to flash v2 itself
   the normal way (Arduino IDE + USB) since the device isn't running v2
   (and therefore doesn't have the OTA task or a backend URL to pull from)
   until after that first flash.
5. **v2.0 → v2.1 (WebOTA → HTTP OTA):** if you have a device running the
   earlier v2.0 browser-WebOTA build, its `access/cmd/ota/enable` payload
   (`{"sha256":...}`) is no longer valid — v2.1 expects
   `{"cmd":"START_HTTP_OTA","version","url","sha256",...}`, see
   docs/09-HTTP-OTA.md. Push this one update manually over USB; every
   update after that can use HTTP OTA.
4. **Node-RED / dashboard**: no changes required for read-only flows
   (tap/door/alert/status events use identical topics and payload shapes).
   If your Node-RED flow issues any of the newly-protected admin commands
   (unlock, enroll, delete card), you'll need to add `ts`/`nonce`/`sig`
   fields — see `docs/03-SECURITY-AUDIT.md` and `security/auth.h` for the
   exact signing scheme. Until you do, those specific commands will be
   rejected (logged to `access/security/event`); read-only dashboard
   panels are unaffected.

## `esp_task_wdt` API version note

`firmware.ino` calls `esp_task_wdt_init(15, true)`, the arduino-esp32
core-2.x signature. On core 3.x, replace it with:

```cpp
esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 15000,
    .idle_core_mask = 0,
    .trigger_panic = true,
};
esp_task_wdt_init(&wdt_config);
```

## Board wiring

Unchanged — same pins as v1 (`config.h`), plus one new optional input:
`PIN_RESET_BUTTON` (GPIO 14) for the manual "hold at boot to provision"
recovery path (Req. #6.4). Leave it unconnected (internal pull-up handles
the default HIGH state) if you don't want to wire a physical button —
triple-reset and the >2min/>10min timers still work without it.

## Folder structure

Matches the brief's recommended layout, with one addition:
`tasks/tasks_common.h` holds the shared queue/event-group/message-struct
declarations that the brief's task files all depend on — it didn't fit
cleanly into any of the other listed folders, so it lives alongside the
task implementations it wires together.
