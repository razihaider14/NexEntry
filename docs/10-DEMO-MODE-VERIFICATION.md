# NexEntry v2.1 — Demo Mode Verification Report

## Status: intact, and now expanded

Demo Mode was checked against every prior NexEntry v2 change (FreeRTOS
migration, ConfigManager/provisioning, admin-command auth, and now the HTTP
OTA migration) and **was not dropped or broken** at any point:

- `services/time_manager.*` has carried `isDemoMode()`/apparent-time logic
  since the original FreeRTOS migration.
- `access/cmd/time` has been wired to it since the same point, handled in
  `services/mqtt_handler.cpp`.
- `drivers/display.cpp` (`demoModeOn()`/`demoModeOff()`) and
  `drivers/feedback.cpp` (`demoMode()`) — the user-visible Demo Mode
  indicators — were untouched by the OTA migration.
- `services/presence.cpp` has always called `TimeManager::now()` (never
  read the RTC/NTP time directly), so check-in/check-out and late-arrival
  logic (`_isLate()`) automatically respect whatever apparent time Demo
  Mode is presenting — this is still true after the v2.1 rewrite.

## What changed in v2.1 (additive, backward-compatible)

The old model was a single "apparent time = real time + fixed offset"
(`addDemoOffset()`). This is now a full clock model in
`services/time_manager.cpp`:

| Requirement | Implementation |
|---|---|
| `demo_enabled` | `TimeManager::isDemoMode()` / `setDemoMode(bool)` — unchanged API name, still there |
| `demo_datetime` | New: `TimeManager::setDemoDateTime(unixTs)` — jump to an absolute apparent time |
| `demo_speed_multiplier` | New: `TimeManager::setDemoSpeedMultiplier(float)` — apparent clock runs at N× real time (e.g. 60.0 = "1 real second = 1 apparent minute", 3600.0 = "1 real second = 1 apparent hour") |
| Pause Demo | New: `TimeManager::pauseDemo()` — freezes the apparent clock |
| Resume Demo | New: `TimeManager::resumeDemo()` — unfreezes from where it was paused |
| Reset Demo | New: `TimeManager::resetDemo()` — re-syncs apparent time to real time, speed back to 1×, without leaving Demo Mode |
| Fast-forward Demo | `TimeManager::fastForward(seconds)` — the old `addDemoOffset()`, renamed and generalized to work whether paused or running |
| Check-in/check-out simulation | Unchanged — `Presence::processTap()` still calls `TimeManager::now()` for every check-in/out timestamp |
| Late-arrival testing | Unchanged — `Presence::_isLate()` still compares `TimeManager::now()` against `LATE_HOUR`/`LATE_MINUTE` |
| "Always knows Production vs Demo" | `access/status` now publishes `demo`, and when `demo:true`, also `demoPaused` and `demoSpeed`, every 30s (`services/mqtt_handler.cpp::publishStatus()`) |

## MQTT payload (backward-compatible)

`access/cmd/time` still accepts the old payload shape unchanged:
```json
{"mode": "DEMO", "offsetSeconds": 3600}
```
and now also accepts the expanded fields:
```json
{"mode": "DEMO", "datetime": 1732960200, "speedMultiplier": 60.0}
{"action": "PAUSE"}
{"action": "RESUME"}
{"action": "RESET"}
{"action": "FAST_FORWARD", "offsetSeconds": -3600}
{"mode": "REAL"}
```
`action` is independent of `mode`, so a dashboard can pause/resume without
re-sending `mode:"DEMO"` every time.

## Future-compatibility note (organization-wide Demo Mode)

The brief asks for this to be "future-compatible with organization-wide
Demo Mode when the backend is implemented." `_handleTimeCommand()`'s doc
comment in `mqtt_handler.cpp` notes a reserved (currently ignored) `scope`
field for this — today the device only understands per-device Demo Mode
(there is no backend yet to coordinate multiple devices), so no `scope`
handling was implemented, just the field-name placeholder and a code
comment marking where org-wide dispatch would plug in once that backend
exists.

## Verification performed

Manual code-path review (no hardware in this environment — see
`docs/07-RESOURCE-USAGE-AND-LIMITATIONS.md`):
- Traced `Presence::processTap()` → `TimeManager::now()` → confirmed it
  returns the demo apparent clock whenever `_demoEnabled` is true, real
  NTP time otherwise.
- Traced `task_display.cpp`'s idle-clock tick → `TimeManager::formatted()`
  → confirmed it renders the apparent (not real) time on the LCD while in
  Demo Mode, matching v1/v2.0 behaviour.
- Confirmed `mqtt_handler.cpp::publishTap()` still includes `"demo":
  TimeManager::isDemoMode()` on every tap event (unchanged field, unchanged
  meaning).
- Confirmed no code path in the HTTP OTA migration touches
  `services/time_manager.*`, `services/presence.cpp`, or the demo-related
  branches of `services/mqtt_handler.cpp`'s command dispatch.
