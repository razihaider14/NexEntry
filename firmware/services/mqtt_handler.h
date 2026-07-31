#pragma once
#include <Arduino.h>
#include "../config.h"
#include "../tasks/tasks_common.h"

namespace MQTT {
    void init();
    void loop();
    bool connected();

    // Drains one MqttPublishMsg from the outbound queue and publishes it.
    // Called repeatedly by task_mqtt.
    void processOutboundMessage(const MqttPublishMsg& msg);

    // Legacy-shaped publishers, kept for parity with v1 and used directly
    // by processOutboundMessage() / internal admin-command handlers.
    void publishTap(const AccessResult& result);
    void publishAlert(const char* type, const char* uid);
    void publishDoorEvent(const char* event);
    void publishStatus();
    void publishEnrollScanned(const char* uid);
    void publishOtaStatus(const char* status, int16_t progress = -1);
    void publishSecurityEvent(const char* topic, const char* reason);
}
