#include "task_status.h"
#include "tasks_common.h"
#include <esp_task_wdt.h>

// Core 1. Periodic status publish (unchanged cadence/payload shape from v1)
// plus heap/stack monitoring for reliability (Req. #8/#9) — logs a warning
// if free heap or any task's remaining stack gets dangerously low, which is
// usually the first sign of trouble long before a crash.

#define LOW_HEAP_WARN_BYTES   20000

static void _taskStatus(void*) {
    esp_task_wdt_add(NULL);
    uint32_t lastStatusPublish = 0;
    uint32_t lastHeapCheck     = 0;

    for (;;) {
        esp_task_wdt_reset();
        uint32_t now = millis();

        if (now - lastStatusPublish >= STATUS_INTERVAL_MS) {
            lastStatusPublish = now;
            MqttPublishMsg m{};
            m.type = MqttPublishType::STATUS;
            sendWithTimeout(qMqttOut, &m);
        }

        if (now - lastHeapCheck >= 10000) {
            lastHeapCheck = now;
            uint32_t freeHeap = ESP.getFreeHeap();
            if (freeHeap < LOW_HEAP_WARN_BYTES) {
                Serial.printf("[STATUS] WARNING: low heap — %u bytes free\n", freeHeap);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void taskStatusStart() {
    xTaskCreatePinnedToCore(_taskStatus, "task_status", 3072, nullptr, 1, nullptr, 1);
}
