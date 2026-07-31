# NexEntry v2 — Security Audit

## Summary of changes vs. v1

| Area | v1 | v2 |
|------|----|----|
| Credentials | Hardcoded in `config.h`, committed to source | Stored in NVS via `ConfigManager`, entered once via captive portal |
| OTA | ArduinoOTA (v1) then browser WebOTA (v2.0) — either open on the network or reachable via a temporary web form | v2.1: device-initiated HTTPS pull only, triggered by an authenticated `START_HTTP_OTA` MQTT command; no listening OTA server exists at any point; SHA-256 integrity check; see docs/09-HTTP-OTA.md |
| Admin MQTT commands | No authentication — anyone who can publish to the broker can unlock the door, delete cards, etc. | HMAC-SHA256 signature + timestamp + nonce (replay protection) required on protected topics |
| MQTT transport | TLS via hardcoded CA cert | Unchanged (TLS via CA cert — now in `config/tls_cert.h`, still compile-time, see note below) |
| Watchdog | None | `esp_task_wdt` on every task |
| Secure Boot / Flash Encryption / NVS Encryption | None | Not implemented (see "Known limitations") — documented integration points only |

## Threat model addressed

1. **Source code leakage exposes credentials** → fixed. No secret is
   compiled into the binary except the CA cert (infrastructure, not a
   per-device secret — see rationale in `config/tls_cert.h`) and the
   `cmdSecret` HMAC key, which is generated on-device with the hardware
   RNG (`esp_random()`) on first boot and never appears in source.
2. **Anyone on the MQTT broker can issue destructive commands** (unlock
   door, delete a card, wipe config, enable a firmware-flashing HTTP
   server) → fixed for the commands listed in the brief: unlock/lock,
   enroll (start/stop/save), delete card, enable OTA, factory reset, open
   provisioning portal. Each requires `ts` (freshness), `nonce` (replay
   protection via a 32-entry ring buffer), and `sig` (HMAC-SHA256 over a
   canonical string, keyed by the per-device `cmdSecret`).
3. **OTA endpoint always reachable** → fixed. `WebServer` only exists
   between an authenticated `ENABLE_WEB_OTA` command and either (a) a
   successful upload + reboot, or (b) a 5-minute timeout, whichever comes
   first.
4. **Firmware image tampering in transit** → mitigated. The `ENABLE_WEB_OTA`
   command carries an expected SHA-256; the uploaded binary is hashed
   during upload and the update is aborted (not applied) on mismatch. This
   protects against corruption/accidental-wrong-file, **not** against a
   sophisticated attacker who can also forge the SHA-256 in the (now
   authenticated) command — that requires a real signature scheme, see
   below.
5. **Replay of a captured "unlock" MQTT message** → fixed by the nonce
   cache + timestamp window (`CMD_MAX_CLOCK_SKEW_S = 60s`).
6. **Watchdog / hang** → `esp_task_wdt` now covers every task; a hung task
   triggers a full reboot rather than a silently-dead device.

## Known gaps / recommendations (not implemented, or partially implemented)

- **No real firmware signature verification.** `OtaSecurity::
  verifyFirmwareSignature()` is a documented no-op hook. SHA-256 protects
  integrity, not authenticity — a compromised MQTT publisher (i.e. anyone
  who can compute a valid HMAC with `cmdSecret`) could still push arbitrary
  firmware. Closing this fully requires an offline code-signing key and a
  signature-verification step, which is impractical to build from scratch
  inside Arduino IDE without a proper build/release pipeline. This is
  explicitly permitted by the brief ("If implementation is impractical
  inside Arduino IDE, leave documented integration points").
- **Secure Boot V2 / Flash Encryption / NVS Encryption are not enabled.**
  These are `idf.py menuconfig` / eFuse-burning operations at the ESP-IDF
  level, not something reachable from Arduino IDE's standard build flow
  without switching to ESP-IDF or using `arduino-cli` with custom
  `sdkconfig` overrides. Enabling them is also **irreversible** (eFuses)
  and would need to happen at the factory-provisioning stage, not as part
  of a firmware refactor. Left as a documented next step; see
  `security/ota_security.h` for the code-level hook this would feed.
- **`access/cmd/time` (demo mode) and `access/cmd/presence/reset` are not
  authenticated.** The brief's protected list is `unlock, enroll, delete
  card, enable OTA, factory reset, open provisioning portal`; these two
  weren't listed, and adding auth would change the payload contract for a
  currently-working dashboard feature (demo mode toggle), which the brief
  also asks to preserve. Recommend adding `ts`/`nonce`/`sig` to these too
  in a follow-up once the dashboard/Node-RED flow can be updated to sign
  them.
- **HMAC key distribution is manual.** The `cmdSecret` generated on first
  provisioning must be copied from the device (there's currently no
  dashboard flow to display/retrieve it) into whatever signs commands
  (e.g. a Node-RED function node). Recommend exposing it once, over the
  captive portal's confirmation page or a one-time authenticated MQTT
  request, rather than requiring serial-console access.
- **TLS CA cert is still compile-time.** Kept out of NVS/the portal
  deliberately (see `config/tls_cert.h`) — it's too large for a mobile
  captive-portal text field and is infrastructure rather than a per-device
  secret. If your broker's cert rotates, that still requires a rebuild +
  an HTTP OTA push (see docs/09-HTTP-OTA.md).
- **Rate limiting is in-RAM only** and resets on reboot; a determined
  attacker who can force reboots could reset their own rate-limit window.
  Low severity given nonce/timestamp checks already block replay, and
  reboots are themselves rate-limited by boot time.

## What was intentionally left unauthenticated (per brief)

RFID scans, attendance events, and presence events (Req. #7: "Do NOT rate
limit... Do NOT change... unless necessary"). These remain exactly as fast
and unthrottled as v1.
