#include "rfid_handler.h"
#include "config.h"
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>

namespace RFID {

    static MFRC522      _rfid(PIN_RC522_SS, PIN_RC522_RST);
    static Preferences  _prefs;
    static CardRecord   _registry[MAX_CARDS];
    static uint8_t      _cardCount   = 0;
    static bool         _enrollMode  = false;

    static String _bytesToUID(byte* buf, byte len) {
        String uid = "";
        for (byte i = 0; i < len; i++) {
            if (buf[i] < 0x10) uid += "0";
            uid += String(buf[i], HEX);
        }
        uid.toUpperCase();
        return uid;
    }

    void init() {
        SPI.begin();
        _rfid.PCD_Init();
        loadRegistry();
        Serial.printf("[RFID] Init OK — %d card(s) in registry\n", _cardCount);
    }

    bool cardPresent() {
        return _rfid.PICC_IsNewCardPresent() && _rfid.PICC_ReadCardSerial();
    }

    String readUID() {
        String uid = _bytesToUID(_rfid.uid.uidByte, _rfid.uid.size);
        _rfid.PICC_HaltA();
        _rfid.PCD_StopCrypto1();
        return uid;
    }

    int lookupCard(const String& uid) {
        for (uint8_t i = 0; i < _cardCount; i++) {
            if (uid.equals(_registry[i].uid)) return i;
        }
        return -1;
    }

    CardRecord getCard(int index) {
        return _registry[index];
    }

    uint8_t getCardCount() {
        return _cardCount;
    }

    bool addCard(const CardRecord& record) {
        if (_cardCount >= MAX_CARDS) {
            Serial.println("[RFID] Registry full, cannot add card");
            return false;
        }

        if (lookupCard(String(record.uid)) != -1) {
            Serial.printf("[RFID] Card %s already exists, use edit instead\n", record.uid);
            return false;
        }

        _registry[_cardCount] = record;
        _cardCount++;
        saveRegistry();
        Serial.printf("[RFID] Card added: %s → %s\n", record.uid, record.name);
        return true;
    }

    bool editCard(const String& uid, const char* newName, bool whitelisted, uint32_t tempExpiry) {

        int idx = lookupCard(uid);
        if (idx == -1) {
            Serial.printf("[RFID] Edit failed — UID %s not found\n", uid.c_str());
            return false;
        }

        strncpy(_registry[idx].name, newName, sizeof(_registry[idx].name) - 1);
        _registry[idx].name[sizeof(_registry[idx].name) - 1] = '\0';
        _registry[idx].whitelisted = whitelisted;
        _registry[idx].tempExpiry  = tempExpiry;

        saveRegistry();
        Serial.printf("[RFID] Card edited: %s → name=%s whitelist=%d expiry=%u\n", uid.c_str(), newName, whitelisted, tempExpiry);
        return true;
    }

    bool deleteCard(const String& uid) {
        int idx = lookupCard(uid);
        if (idx == -1) {
            Serial.printf("[RFID] Delete failed — UID %s not found\n", uid.c_str());
            return false;
        }

        for (uint8_t i = idx; i < _cardCount - 1; i++) {
            _registry[i] = _registry[i + 1];
        }
        _cardCount--;

        memset(&_registry[_cardCount], 0, sizeof(CardRecord));

        saveRegistry();
        Serial.printf("[RFID] Card deleted: %s\n", uid.c_str());
        return true;
    }

    void saveRegistry() {
        _prefs.begin(NVS_NAMESPACE, false); 
        _prefs.putBytes(NVS_KEY_REGISTRY, _registry, sizeof(_registry));
        _prefs.putUChar("card_count", _cardCount);
        _prefs.end();
        Serial.printf("[RFID] Registry saved to NVS (%d cards)\n", _cardCount);
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

    bool isEnrollMode() {
        return _enrollMode;
    }

} 