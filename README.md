# NexEntry v2

Same door, same badge readers, same dashboard — rebuilt internals.

v2 is a from-source modernization of the original NexEntry firmware
(`README-v1.md`, kept for reference): FreeRTOS task architecture, secure
provisioning via a captive portal (no more hardcoded WiFi/MQTT
credentials), a Secure HTTP OTA flow that replaces ArduinoOTA/browser WebOTA
(the device pulls firmware over HTTPS from a backend URL — no listening OTA
server, ever), and
authenticated administrative MQTT commands.

**Start here:**
1. `docs/01-ARCHITECTURE.md` — how it's put together
2. `docs/02-TASK-DIAGRAM.md` — the FreeRTOS task/queue map
3. `docs/06-CONFIGURATION-AND-PROVISIONING.md` — first-boot setup
4. `docs/05-MIGRATION-NOTES.md` — upgrading an existing v1 device
5. `docs/03-SECURITY-AUDIT.md` — what's protected, what isn't yet
6. `docs/04-DEPENDENCIES.md` — libraries to install
7. `docs/07-RESOURCE-USAGE-AND-LIMITATIONS.md` — flash/RAM estimates, known gaps
8. `docs/08-SAMPLE-COMMAND-SIGNER.md` — signing admin MQTT commands from Node-RED
9. `docs/09-HTTP-OTA.md` — Secure HTTP OTA flow, task diagram, MQTT payload spec
10. `docs/10-DEMO-MODE-VERIFICATION.md` — Demo Mode status + expanded controls

`Dashboard/`, `Images/`, and `BOM.csv` are unchanged from v1 — the
dashboard talks to v2 over the exact same MQTT topics and payload shapes.

This was written and reviewed for correctness but **not compiled on real
hardware** in this environment — see "Verifying this build" in
`docs/07-RESOURCE-USAGE-AND-LIMITATIONS.md` before deploying.
