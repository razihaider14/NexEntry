#pragma once
#include <Arduino.h>
// Provisioning portal (Req. #5/#6). blockingRunProvisioningPortal() is used
// for the mandatory first-boot flow (called directly from setup(), before
// any other task exists — there's nothing to provision from yet).
// requestOpenProvisioningPortal() is used for every other trigger (WiFi/MQTT
// down, triple reset, button, authenticated MQTT command) once the system
// is already running, and spins up a dynamic task instead of blocking.
void blockingRunProvisioningPortal();
void requestOpenProvisioningPortal();
