# NexEntry v2 — Resource Usage & Known Limitations

## Memory / flash usage comparison

**Caveat up front: these are estimates, not measured numbers.** This
environment can't run the Arduino/ESP32 toolchain to produce a real
`.elf`/map file, so treat the figures below as directional, not exact.
Get real numbers with `arduino-cli compile --verbose` or the IDE's
"Sketch → Export compiled Binary" output, which prints exact flash/RAM
usage per build.

| Metric | v1 (estimate) | v2 (estimate) | Why it moves |
|---|---|---|---|
| Flash (program) | ~950 KB–1.1 MB | ~1.3–1.5 MB | + WiFiManager, + HTTPClient/WiFiClientSecure/Update (OTA), + mbedtls SHA/HMAC usage, + FreeRTOS task overhead (small — FreeRTOS is already always linked in on ESP32/Arduino, v1 uses it implicitly via `loop()`/WiFi/etc.) |
| RAM (static + heap headroom needed) | ~40–50 KB in use, rest free of ~320 KB | ~55–70 KB in use | + 8 task stacks (2.5–8 KB each, see table below) + several small queues (all fixed-size, see `tasks_common.h`) + WiFiManager's AP+web server only *while provisioning is active* (transient) |

**v2.0 → v2.1 (WebOTA → HTTP OTA) note:** removing the `WebServer`-based
browser upload endpoint and adding `HTTPClient` is roughly a wash on flash
(both are already-linked-in ESP32-core libraries, similar footprint) and
slightly *reduces* peak RAM during an OTA session — WebOTA ran an HTTP
*server* (accepting connections, buffering multipart form data) for up to 5
minutes regardless of whether anyone connected; HTTP OTA only opens a
single outbound HTTPS *client* connection for the duration of the actual
download, and the 1 KB streaming buffer in `task_ota.cpp` is the only
per-chunk allocation.
| Heap fragmentation risk | Moderate — `String` used throughout (`display.cpp`, `mqtt_handler.cpp`, `rfid_handler.cpp` in v1) | Lower — v2 replaced `String` with fixed `char[]` buffers + `snprintf`/`strlcpy` everywhere in the hot paths (RFID UID, presence, MQTT payloads, time formatting) per Req. #9 |

### Task stack sizes (as configured in `tasks/*.cpp`)

| Task | Stack (bytes) |
|---|---|
| task_rfid | 6144 |
| task_door | 3072 |
| task_display | 4096 |
| task_feedback | 2560 |
| task_wifi | 4096 |
| task_mqtt | 6144 |
| task_status | 3072 |
| task_ota (transient) | 8192 |
| task_provisioning (transient) | 6144 |

These are starting points, not tuned minimums — Req. #8 asks for stack
monitoring; add `uxTaskGetStackHighWaterMark()` logging (e.g. inside
`task_status`) during bring-up and trim oversized stacks once you have real
high-water-mark data from your hardware.

## Known limitations

1. **Not compiled/tested on hardware.** This refactor was written and
   reviewed for correctness but not built or flashed — see "Verifying this
   build" below before relying on it.
2. **No true firmware signing** — SHA-256 integrity only (see security
   audit).
2a. **Rollback is best-effort.** `esp_ota_mark_app_valid_cancel_rollback()`
    is called at boot (`firmware.ino`), but real automatic
    rollback-on-crash requires `CONFIG_APP_ROLLBACK_ENABLE` in the
    ESP-IDF sdkconfig, which isn't exposed by stock Arduino IDE. In
    practice: a bad OTA image that boot-loops will eventually trip the
    triple-reset provisioning path (Req. #6.4) rather than being
    automatically reverted to the previous image — see docs/09-HTTP-OTA.md.
2b. **HTTP OTA download has no resume support.** A dropped connection
    mid-download aborts the update; the device stays on its current
    firmware and the admin has to re-issue `START_HTTP_OTA`.
3. **Secure Boot V2 / Flash Encryption / NVS Encryption** are not enabled —
   documented hooks only (see security audit).
4. **`WiFiManagerParameter` values are plain `char*`**, including the OTA
   password field in the portal HTML — WiFiManager doesn't support masked
   custom-parameter fields out of the box. Anyone who can see the phone
   screen during provisioning can read it. Low risk (physical proximity
   already implies significant access during setup) but worth knowing.
5. **`cmdSecret` distribution is manual** (see security audit) — no
   dashboard flow to fetch it yet.
6. **CA certificate is still compile-time** — rotating your broker's CA
   requires a rebuild + OTA push, not a portal field (see rationale in
   `config/tls_cert.h`).
7. **Rate limiting resets on reboot** (in-RAM only).
8. **Time-based recovery triggers (2 min / 10 min) use `millis()`**, so
   they reset on any reboot — a device stuck in a WiFi-down boot loop
   wouldn't reliably hit the 2-minute mark if it keeps rebooting for other
   reasons. Watchdog panics are the more likely trigger in that scenario
   and will themselves eventually surface as repeated boots, which trips
   the triple-reset provisioning path.
9. **Single MQTT client / single WiFi radio** — as on v1, if the broker
   requires a very large TLS handshake buffer or the AP is congested,
   connect time is unchanged from v1's baseline.

## Verifying this build

Before deploying:
1. Open `firmware/firmware.ino` in Arduino IDE with the libraries in
   `docs/04-DEPENDENCIES.md` installed.
2. Compile for your exact board (`Tools → Board`) and confirm 0 errors —
   flag anything symbol/version-specific (the `esp_task_wdt_init` signature
   is the most likely one, see migration notes) if your core version
   differs from what this was written against.
3. Flash a test unit, walk through first-boot provisioning, then test each
   admin MQTT command end-to-end with a signed payload before wiring it
   into production Node-RED flows.
