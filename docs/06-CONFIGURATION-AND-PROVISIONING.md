# NexEntry v2 — Configuration & Provisioning

## First boot

1. Flash v2 over USB (Arduino IDE, as with v1).
2. Device finds no config in NVS and opens a WiFi access point named
   **`NexEntry-Setup`**.
3. Connect to it from a phone/laptop; a captive portal should open
   automatically (or browse to `192.168.4.1`).
4. Tap **Configure WiFi**, pick your network, and fill in the extra fields:
   - WiFi SSID / Password (native WiFiManager fields)
   - MQTT Host, MQTT Port, MQTT Username, MQTT Password
   - Device Name
   - OTA Username, OTA Password
5. Submit. The device saves everything to NVS and reboots into normal
   operation.

## Recovery modes (auto-opens the same portal)

| Trigger | Condition |
|---|---|
| No configuration | `ConfigManager::isConfigured() == false` |
| WiFi down | Disconnected continuously for >2 minutes |
| MQTT down | Disconnected continuously for >10 minutes |
| Triple reset | 3 power/reset cycles within 10 seconds of each other |
| Reset button | `PIN_RESET_BUTTON` held LOW at boot |
| Remote command | Authenticated MQTT command on `access/cmd/provision/open` |

The portal auto-closes after 5 minutes if nobody connects, and the device
resumes whatever it was doing before (reconnecting WiFi/MQTT with the
existing config) rather than getting stuck.

## Rotating credentials

- **WiFi / MQTT / device name / OTA password**: reopen the portal (any
  trigger above) and resubmit — existing values are pre-filled except OTA
  password (leave blank to keep the current one).
- **`cmdSecret` (HMAC key for admin commands)**: generated once on first
  provisioning and preserved across re-provisioning. There's currently no
  UI to rotate it short of a full `ConfigManager::factoryReset()` (which
  also clears WiFi/MQTT config, forcing a full re-provision) — see the
  security audit's recommendations for a nicer rotation flow.

## Administrative MQTT commands (all require ts/nonce/sig — see
`security/auth.h`)

| Topic | Payload | Effect |
|---|---|---|
| `access/cmd/enroll` | `{"cmd":"START"\|"STOP", ...}` | Enter/exit enrollment mode |
| `access/cmd/enroll/save` | `{"uid","name","whitelisted","tempExpiry", ...}` | Save a scanned card |
| `access/cmd/card/edit` | `{"uid","name","whitelisted","tempExpiry", ...}` | Edit an existing card |
| `access/cmd/card/delete` | `{"uid", ...}` | Delete a card |
| `access/cmd/door` | `{"cmd":"UNLOCK"\|"LOCK", ...}` | Remote door control |
| `access/cmd/ota/enable` | `{"cmd":"START_HTTP_OTA","version","url","sha256","signature","force", ...}` | Downloads and installs firmware over HTTPS — see docs/09-HTTP-OTA.md |
| `access/cmd/factory_reset` | `{...}` | Wipes network/auth config, reboots to provisioning |
| `access/cmd/provision/open` | `{...}` | Opens the portal without waiting for a timeout |

Rejections are published to `access/security/event` with a `reason` field
(`"missing sig"`, `"bad signature"`, `"replayed nonce"`, `"rate limited"`,
etc.) for dashboard/alerting visibility.

## Unauthenticated (unchanged from v1)

`access/cmd/time` (demo mode) and `access/cmd/presence/reset` — see the
security audit for why these were left as-is.
