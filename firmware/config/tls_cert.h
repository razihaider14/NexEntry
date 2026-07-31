#pragma once
// ---------------------------------------------------------------------------
// tls_cert.h — MQTT broker CA certificate.
//
// This is intentionally NOT moved into NVS/the captive portal like the other
// credentials in Req. #4. A CA cert is deployment infrastructure (tied to
// your broker's TLS setup), not a per-device secret, and it's too large for
// a portal text field to be practical/safe to type on a phone. It still
// changes per deployment — replace the placeholder below at build time.
//
// If you need to change the CA without reflashing, the WebOTA path (Req #2)
// is the intended route: ship a new build with the updated cert.
// ---------------------------------------------------------------------------
#include <Arduino.h>

static const char CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
your_ca_cert_here
-----END CERTIFICATE-----
)EOF";
