#include "rfid_handler.h"
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace RFID {

    static MFRC522      _rfid(PIN_RC522_SS, PIN_RC522_RST);
    static Preferences  _prefs;
    static CardRecord   _registry[MAX_CARDS];
    static uint8_t      _cardCount   = 0;
    static volatile bool _enrollMode = false;
    static SemaphoreHandle_t _regMutex = nullptr;

    static void _bytesToUID(byte* buf, byte len, char* out, size_t outLen) {
        out[0] = '\0';
        char tmp[3];
        for (byte i = 0; i < len; i++) {
            snprintf(tmp, sizeof(tmp), "%02X", buf[i]);
            strlcat(out, tmp, outLen);
        }
    }

    void init() {
        if (!_regMutex) _regMutex = xSemaphoreCreateMutex();
        SPI.begin();
        _rfid.PCD_Init();
        loadRegistry();
        Serial.printf("[RFID] Init OK — %d card(s) in registry\n", _cardCount);
    }

    bool cardPresent() {
        return _rfid.PICC_IsNewCardPresent() && _rfid.PICC_ReadCardSerial();
    }

    void readUID(char* out, size_t outLen) {
        _bytesToUID(_rfid.uid.uidByte, _rfid.uid.size, out, outLen);
        _rfid.PICC_HaltA();
        _rfid.PCD_StopCrypto1();
    }

    int lookupCard(const char* uid) {
        xSemaphoreTake(_regMutex, portMAX_DELAY);
        int found = -1;
        for (uint8_t i = 0; i < _cardCount; i++) {
            if (strcmp(uid, _registry[i].uid) == 0) { found = i; break; }
        }
        xSemaphoreGive(_regMutex);
        return found;
    }

    CardRecord getCard(int index) {
        xSemaphoreTake(_regMutex, portMAX_DELAY);
        CardRecord c = _registry[index];
        xSemaphoreGive(_regMutex);
        return c;
    }

    uint8_t getCardCount() {
        xSemaphoreTake(_regMutex, portMAX_DELAY);
        uint8_t n = _cardCount;
        xSemaphoreGive(_regMutex);
        return n;
    }

    bool addCard(const CardRecord& record) {
        xSemaphoreTake(_regMutex, portMAX_DELAY);
        if (_cardCount >= MAX_CARDS) {
            xSemaphoreGive(_regMutex);
            Serial.println("[RFID] Registry full, cannot add card");
            return false;
        }
        for (uint8_t i = 0; i < _cardCount; i++) {
            if (strcmp(record.uid, _registry[i].uid) == 0) {
                xSemaphoreGive(_regMutex);
                Serial.printf("[RFID] Card %s already exists, use edit instead\n", record.uid);
                return false;
            }
        }
        _registry[_cardCount] = record;
        _cardCount++;
        xSemaphoreGive(_regMutex);
        saveRegistry();
        Serial.printf("[RFID] Card added: %s -> %s\n", record.uid, record.name);
        return true;
    }

    bool editCard(const char* uid, const char* newName, bool whitelisted, uint32_t tempExpiry) {
        xSemaphoreTake(_regMutex, portMAX_DELAY);
        int idx = -1;
        for (uint8_t i = 0; i < _cardCount; i++) {
            if (strcmp(uid, _registry[i].uid) == 0) { idx = i; break; }
        }
        if (idx == -1) {
            xSemaphoreGive(_regMutex);
            Serial.printf("[RFID] Edit failed — UID %s not found\n", uid);
            return false;
        }
        strncpy(_registry[idx].name, newName, sizeof(_registry[idx].name) - 1);
        _registry[idx].name[sizeof(_registry[idx].name) - 1] = '\0';
        _registry[idx].whitelisted = whitelisted;
        _registry[idx].tempExpiry  = tempExpiry;
        xSemaphoreGive(_regMutex);
        saveRegistry();
        Serial.printf("[RFID] Card edited: %s\n", uid);
        return true;
    }

    bool deleteCard(const char* uid) {
        xSemaphoreTake(_regMutex, portMAX_DELAY);
        int idx = -1;
        for (uint8_t i = 0; i < _cardCount; i++) {
            if (strcmp(uid, _registry[i].uid) == 0) { idx = i; break; }
        }
        if (idx == -1) {
            xSemaphoreGive(_regMutex);
            Serial.printf("[RFID] Delete failed — UID %s not found\n", uid);
            return false;
        }
        for (uint8_t i = idx; i < _cardCount - 1; i++) _registry[i] = _registry[i + 1];
        _cardCount--;
        memset(&_registry[_cardCount], 0, sizeof(CardRecord));
        xSemaphoreGive(_regMutex);
        saveRegistry();
        Serial.printf("[RFID] Card deleted: %s\n", uid);
        return true;
    }

    void saveRegistry() {
        xSemaphoreTake(_regMutex, portMAX_DELAY);
        _prefs.begin(NVS_NAMESPACE, false);
        _prefs.putBytes(NVS_KEY_REGISTRY, _registry, sizeof(_registry));
        _prefs.putUChar("card_count", _cardCount);
        _prefs.end();
        uint8_t n = _cardCount;
        xSemaphoreGive(_regMutex);
        Serial.printf("[RFID] Registry saved to NVS (%d cards)\n", n);
    }

    void loadRegistry() {
        _prefs.begin(NVS_NAMESPACE, true);
        size_t len = _prefs.getBytesLength(NVS_KEY_REGISTRY);
        if (len == sizeof(_registry)) {
            _prefs.getBytes(NVS_KEY_REGISTRY, _registry, sizeof(_registry));
            _cardCount = _prefs.getUChar("card_count", 0);
            Serial.printf("[RFID] Registry loaded from NVS (%d cards)\n", _cardCount);
        } else {
            memset(_registry, 0, sizeof(_registry));
            _cardCount = 0;
            Serial.println("[RFID] No registry in NVS — starting fresh");
        }
        _prefs.end();
    }

    void setEnrollMode(bool on) {
        _enrollMode = on;
        Serial.printf("[RFID] Enrollment mode %s\n", on ? "ON" : "OFF");
    }

    bool isEnrollMode() { return _enrollMode; }
}
