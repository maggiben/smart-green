#include <RTClib.h> // Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include <EEPROM.h>
#include "constants.h"

#define SETTINGS_ADDRESS 0 /* Active plants (battery backed ram address) */
#define SETTING_MAX_NAME 64
#define SETTINGS_MAX_ALARMS 10
#define SETTINGS_ALARM_DATA_STORE 4

struct Settings {
  // char * name;
  char name[SETTING_MAX_NAME];
  uint8_t id;
  uint32_t lastDateTimeSync;
  uint32_t updatedOn;
  uint8_t alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_DATA_STORE];
};

void saveSettings(Settings* settings);
void readSettings(Settings* settings);
