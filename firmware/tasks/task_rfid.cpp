#include "task_rfid.h"
#include "tasks_common.h"
#include "../drivers/rfid_handler.h"
#include "../services/presence.h"
#include <esp_task_wdt.h>

// This task owns the full v1 "handleTap" behaviour, now fanning results out
// over queues instead of calling Display/Feedback/MQTT directly. RFID scans,
// attendance and presence logic are NEVER blocked by WiFi/MQTT state
// (Req. #8) — pushes to qMqttOut use a short timeout and are simply dropped
// (with a log line) if the MQTT task is backed up, rather than blocking.

static void _handleTap(const char* uid, int cardIndex) {
    if (RFID::isEnrollMode()) {
        DisplayMsg dm{DisplayMsgType::ENROLL_SCANNED};
        strlcpy(dm.text, uid, sizeof(dm.text));
        sendWithTimeout(qDisplay, &dm);

        FeedbackMsg fm{FeedbackMsgType::ENROLL_READY};
        sendWithTimeout(qFeedback, &fm);

        MqttPublishMsg mm{};
        mm.type = MqttPublishType::ENROLL_SCANNED;
        strlcpy(mm.text, uid, sizeof(mm.text));
        sendWithTimeout(qMqttOut, &mm);
        return;
    }

    bool granted = Presence::processTap(cardIndex, uid);
    AccessResult result = Presence::getLastResult();

    if (granted) {
        DisplayMsg dm{};
        dm.type = (strcmp(result.action, "CHECK_IN") == 0) ? DisplayMsgType::WELCOME : DisplayMsgType::GOODBYE;
        strlcpy(dm.text, result.name, sizeof(dm.text));
        sendWithTimeout(qDisplay, &dm);

        FeedbackMsg fm{FeedbackMsgType::GRANTED};
        sendWithTimeout(qFeedback, &fm);

        DoorCmdMsg dc{DoorCmdType::UNLOCK, DOOR_UNLOCK_MS};
        sendWithTimeout(qDoorCmd, &dc);

        MqttPublishMsg mm{};
        mm.type = MqttPublishType::TAP;
        mm.access = result;
        sendWithTimeout(qMqttOut, &mm);

    } else {
        DisplayMsg dm{};
        FeedbackMsg fm{};
        bool haveAlert = false;
        char alertType[20] = {0};

        if (strcmp(result.access, "UNKNOWN") == 0) {
            dm.type = DisplayMsgType::UNKNOWN_CARD;
            fm.type = FeedbackMsgType::UNKNOWN_CARD;
            haveAlert = true;
            strlcpy(alertType, "UNKNOWN_CARD", sizeof(alertType));
        } else if (strcmp(result.access, "DENIED_BLACKLIST") == 0) {
            dm.type = DisplayMsgType::DENIED;
            strlcpy(dm.text, "Blacklisted", sizeof(dm.text));
            fm.type = FeedbackMsgType::DENIED;
        } else if (strcmp(result.access, "DENIED_EXPIRED") == 0) {
            dm.type = DisplayMsgType::DENIED;
            strlcpy(dm.text, "Access Expired", sizeof(dm.text));
            fm.type = FeedbackMsgType::DENIED;
        }

        sendWithTimeout(qDisplay, &dm);
        sendWithTimeout(qFeedback, &fm);

        if (haveAlert) {
            MqttPublishMsg mm{};
            mm.type = MqttPublishType::ALERT;
            strlcpy(mm.alertType, alertType, sizeof(mm.alertType));
            strlcpy(mm.alertUid, uid, sizeof(mm.alertUid));
            sendWithTimeout(qMqttOut, &mm);
        } else {
            MqttPublishMsg mm{};
            mm.type = MqttPublishType::TAP;
            mm.access = result;
            sendWithTimeout(qMqttOut, &mm);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    DisplayMsg idleMsg{DisplayMsgType::IDLE};
    sendWithTimeout(qDisplay, &idleMsg);
}

static void _taskRfid(void*) {
    esp_task_wdt_add(NULL);
    RFID::init();
    Presence::init();

    char     lastCardUID[12] = {0};
    uint32_t lastCardTime    = 0;

    for (;;) {
        esp_task_wdt_reset();

        if (RFID::cardPresent()) {
            char uid[12];
            RFID::readUID(uid, sizeof(uid));

            uint32_t now      = millis();
            bool     sameCard = strcmp(uid, lastCardUID) == 0;
            bool     tooSoon  = (now - lastCardTime) < DEBOUNCE_MS;

            if (!(sameCard && tooSoon)) {
                strlcpy(lastCardUID, uid, sizeof(lastCardUID));
                lastCardTime = now;
                int cardIndex = RFID::lookupCard(uid);
                _handleTap(uid, cardIndex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // RC522 poll interval
    }
}

void taskRfidStart() {
    // Larger stack: this task calls into Presence/RFID and builds several
    // queue messages per tap.
    xTaskCreatePinnedToCore(_taskRfid, "task_rfid", 6144, nullptr, 4, nullptr, 0);
}
