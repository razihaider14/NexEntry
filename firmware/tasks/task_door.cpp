#include "task_door.h"
#include "tasks_common.h"
#include "../drivers/door.h"
#include <esp_task_wdt.h>

static void _publishDoorEvent(const char* evt) {
    MqttPublishMsg m{};
    m.type = MqttPublishType::DOOR_EVENT;
    strlcpy(m.doorEvent, evt, sizeof(m.doorEvent));
    sendWithTimeout(qMqttOut, &m);
}

static void _taskDoor(void*) {
    esp_task_wdt_add(NULL);
    Door::init();

    DoorCmdMsg cmd;
    bool wasHeldOpen = false;

    for (;;) {
        esp_task_wdt_reset();

        if (xQueueReceive(qDoorCmd, &cmd, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (cmd.type == DoorCmdType::UNLOCK) {
                Door::unlock(cmd.durationMs);
                _publishDoorEvent("UNLOCKED");
            } else {
                Door::lock();
                _publishDoorEvent("LOCKED");
            }
        }

        Door::tick();

        if (Door::autoRelockFired()) {
            _publishDoorEvent("LOCKED");
        }

        bool heldOpen = Door::isHeldOpen();
        if (heldOpen && !wasHeldOpen) {
            FeedbackMsg fm{FeedbackMsgType::DOOR_HELD_ON};
            sendWithTimeout(qFeedback, &fm);

            MqttPublishMsg m{};
            m.type = MqttPublishType::ALERT;
            strlcpy(m.alertType, "DOOR_HELD_OPEN", sizeof(m.alertType));
            m.alertUid[0] = '\0';
            sendWithTimeout(qMqttOut, &m);
            _publishDoorEvent("HELD_OPEN");
        }
        wasHeldOpen = heldOpen;

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void taskDoorStart() {
    xTaskCreatePinnedToCore(_taskDoor, "task_door", 3072, nullptr, 3, nullptr, 0);
}
