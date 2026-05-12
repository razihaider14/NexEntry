#pragma once

#include <Arduino.h>

#define WIFI_SSID       "your_ssid"
#define WIFI_PASSWORD   "your_password"

#define MQTT_BROKER     "your_pi_ip"
#define MQTT_USER       "your_mqtt_user"
#define MQTT_PASSWORD   "your_mqtt_password"
#define MQTT_PORT          8883

static const char CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
your_ca_cert_here
-----END CERTIFICATE-----
)EOF";

#define TOPIC_TAP                "access/tap"
#define TOPIC_DOOR               "access/door"
#define TOPIC_ALERT              "access/alert"
#define TOPIC_STATUS             "access/status"
#define TOPIC_ENROLL_SCANNED     "access/enroll/scanned"

#define TOPIC_CMD_ENROLL         "access/cmd/enroll"
#define TOPIC_CMD_ENROLL_SAVE    "access/cmd/enroll/save"
#define TOPIC_CMD_CARD_EDIT      "access/cmd/card/edit"
#define TOPIC_CMD_CARD_DELETE    "access/cmd/card/delete"
#define TOPIC_CMD_TIME           "access/cmd/time"
#define TOPIC_CMD_DOOR           "access/cmd/door"
#define TOPIC_CMD_PRESENCE_RESET "access/cmd/presence/reset"

#define NTP_SERVER         "pool.ntp.org"
#define NTP_GMT_OFFSET     18000        
#define NTP_DST_OFFSET     0

#define PIN_RC522_SS       5
#define PIN_RC522_RST      4
#define PIN_LCD_SDA        21
#define PIN_LCD_SCL        22
#define PIN_LED_GREEN      26
#define PIN_LED_RED        25
#define PIN_BUZZER         27
#define PIN_DOOR           13

#define LCD_ADDRESS        0x27
#define LCD_COLS           16
#define LCD_ROWS           2

#define DOOR_UNLOCK_MS     3000         
#define DOOR_HELD_OPEN_MS  10000       
#define SERVO_LOCKED_ANGLE    0
#define SERVO_UNLOCKED_ANGLE  90

#define STATUS_INTERVAL_MS 30000        
#define DEBOUNCE_MS        2000       

#define MAX_CARDS          15          
#define NVS_NAMESPACE      "access_ctrl"
#define NVS_KEY_REGISTRY   "card_reg"
#define NVS_KEY_PRESENCE   "presence"

#define LATE_HOUR          9           
#define LATE_MINUTE        0


struct CardRecord {
    char     uid[12];        
    char     name[24];       
    bool     whitelisted;    
    uint32_t tempExpiry;    
};

struct PersonState {
    bool     isInside;
    uint32_t checkInTime;    
};

struct AccessResult {
    int      cardIndex;     
    char     uid[12];
    char     name[24];
    char     action[12];    
    char     access[20];    
    uint32_t timestamp;
    bool     isLate;       
};