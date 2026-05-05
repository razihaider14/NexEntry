// ============================================================
//  display.cpp
// ============================================================

#include "display.h"
#include "config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

namespace Display {

    static LiquidCrystal_I2C _lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
    static bool _isIdle = false;   // true only when idle screen is active
                                   // prevents updateTime() writing over other screens

    // ── Helpers ─────────────────────────────────────────────

    // Clears and prints two lines cleanly
    // Pads with spaces to overwrite any leftover characters
    static void _print(const char* line1, const char* line2) {
        _isIdle = false;

        char buf1[LCD_COLS + 1];
        char buf2[LCD_COLS + 1];

        // Left-justify and space-pad to full LCD width
        snprintf(buf1, sizeof(buf1), "%-16s", line1);
        snprintf(buf2, sizeof(buf2), "%-16s", line2);

        _lcd.setCursor(0, 0);
        _lcd.print(buf1);
        _lcd.setCursor(0, 1);
        _lcd.print(buf2);
    }

    // Truncates a name to fit LCD width cleanly
    static void _safeName(const char* name, char* out, uint8_t maxLen) {
        strncpy(out, name, maxLen - 1);
        out[maxLen - 1] = '\0';
    }

    // ── Public ───────────────────────────────────────────────

    void init() {
        Wire.begin(PIN_LCD_SDA, PIN_LCD_SCL);
        _lcd.init();
        _lcd.backlight();
        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print("  Access Control");
        _lcd.setCursor(0, 1);
        _lcd.print("  Initializing..");
        Serial.println("[DISPLAY] Init OK");
    }

    void idle() {
        char buf1[LCD_COLS + 1];
        char buf2[LCD_COLS + 1];
        snprintf(buf1, sizeof(buf1), "%-16s", "Scan Card...");
        snprintf(buf2, sizeof(buf2), "%-16s", "");

        _lcd.setCursor(0, 0);
        _lcd.print(buf1);
        _lcd.setCursor(0, 1);
        _lcd.print(buf2);

        _isIdle = true;   // allow updateTime() to write to line 2
    }

    void welcome(const char* name) {
        char safeName[LCD_COLS + 1];
        _safeName(name, safeName, sizeof(safeName));
        _print("Welcome!", safeName);
    }

    void goodbye(const char* name) {
        char safeName[LCD_COLS + 1];
        _safeName(name, safeName, sizeof(safeName));
        _print("Goodbye!", safeName);
    }

    void denied(const char* reason) {
        _print("Access Denied", reason);
    }

    void unknown() {
        _print("Unknown Card", "Not Registered");
    }

    void enrollMode() {
        _print("Enroll Mode", "Scan a card...");
    }

    void enrollScanned(const char* uid) {
        char safeUID[LCD_COLS + 1];
        _safeName(uid, safeUID, sizeof(safeUID));
        _print("Card Scanned!", safeUID);
    }

    void enrollSaved(const char* name) {
        char safeName[LCD_COLS + 1];
        _safeName(name, safeName, sizeof(safeName));
        _print("Card Saved!", safeName);
    }

    void demoModeOn() {
        _print("Demo Mode ON", "Time overridden");
    }

    void demoModeOff() {
        _print("Demo Mode OFF", "Real time active");
    }

    void connecting() {
        _print("Connecting...", "Please wait");
    }

    void mqttReconnecting() {
        _print("MQTT...", "Reconnecting");
    }

    void updateTime(const String& timeStr) {
        // Only updates line 2 when idle screen is active
        // Prevents clock from overwriting tap feedback messages
        if (!_isIdle) return;

        char buf[LCD_COLS + 1];
        snprintf(buf, sizeof(buf), "%-16s", timeStr.c_str());
        _lcd.setCursor(0, 1);
        _lcd.print(buf);
    }

} // namespace Display