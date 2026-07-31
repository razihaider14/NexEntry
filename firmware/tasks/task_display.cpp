#include "task_display.h"
#include "tasks_common.h"
#include "../drivers/display.h"
#include "../services/time_manager.h"
#include <esp_task_wdt.h>

static void _taskDisplay(void*) {
    esp_task_wdt_add(NULL);
    Display::init();
    Display::idle();

    DisplayMsg msg;
    uint32_t lastClockTick = 0;

    for (;;) {
        esp_task_wdt_reset();

        if (xQueueReceive(qDisplay, &msg, pdMS_TO_TICKS(200)) == pdTRUE) {
            switch (msg.type) {
                case DisplayMsgType::IDLE:              Display::idle(); break;
                case DisplayMsgType::WELCOME:            Display::welcome(msg.text); break;
                case DisplayMsgType::GOODBYE:            Display::goodbye(msg.text); break;
                case DisplayMsgType::DENIED:             Display::denied(msg.text); break;
                case DisplayMsgType::UNKNOWN_CARD:       Display::unknown(); break;
                case DisplayMsgType::ENROLL_MODE:        Display::enrollMode(); break;
                case DisplayMsgType::ENROLL_SCANNED:     Display::enrollScanned(msg.text); break;
                case DisplayMsgType::ENROLL_SAVED:       Display::enrollSaved(msg.text); break;
                case DisplayMsgType::DEMO_ON:            Display::demoModeOn(); break;
                case DisplayMsgType::DEMO_OFF:           Display::demoModeOff(); break;
                case DisplayMsgType::CONNECTING:         Display::connecting(); break;
                case DisplayMsgType::MQTT_RECONNECTING:  Display::mqttReconnecting(); break;
                case DisplayMsgType::PROVISIONING:       Display::provisioning(); break;
                case DisplayMsgType::OTA_ACTIVE:          Display::otaActive(); break;
            }
        }

        // Idle-screen clock tick (matches v1's 1s update-while-idle behaviour)
        uint32_t now = millis();
        if (now - lastClockTick >= 1000) {
            lastClockTick = now;
            char buf[20];
            TimeManager::formatted(buf, sizeof(buf));
            Display::updateTime(buf);
        }
    }
}

void taskDisplayStart() {
    xTaskCreatePinnedToCore(_taskDisplay, "task_display", 4096, nullptr, 2, nullptr, 0);
}
