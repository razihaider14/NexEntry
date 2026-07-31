#include "task_feedback.h"
#include "tasks_common.h"
#include "../drivers/feedback.h"
#include <esp_task_wdt.h>

static void _taskFeedback(void*) {
    esp_task_wdt_add(NULL);
    Feedback::init();

    FeedbackMsg msg;
    for (;;) {
        esp_task_wdt_reset();
        if (xQueueReceive(qFeedback, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (msg.type) {
                case FeedbackMsgType::GRANTED:      Feedback::granted(); break;
                case FeedbackMsgType::DENIED:       Feedback::denied(); break;
                case FeedbackMsgType::UNKNOWN_CARD: Feedback::unknown(); break;
                case FeedbackMsgType::ENROLL_READY: Feedback::enrollReady(); break;
                case FeedbackMsgType::ENROLL_SAVED: Feedback::enrollSaved(); break;
                case FeedbackMsgType::DEMO_MODE:    Feedback::demoMode(); break;
                case FeedbackMsgType::DOOR_HELD_ON: Feedback::doorHeldOn(); break;
                case FeedbackMsgType::DOOR_HELD_OFF: Feedback::doorHeldOff(); break;
            }
        }
        // Non-blocking pattern tick for the "held open" LED/buzzer pattern.
        Feedback::tick();
    }
}

void taskFeedbackStart() {
    xTaskCreatePinnedToCore(_taskFeedback, "task_feedback", 2560, nullptr, 2, nullptr, 0);
}
