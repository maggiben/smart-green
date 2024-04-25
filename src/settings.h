#include <RTClib.h> // Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include "constants.h"

#define SETTINGS_ADDRESS 0 /* Active plants (battery backed ram address) */
#define SETTINGS_MAX_ALARMS 10
#define SETTINGS_ALARM_DATA_STORE 4

struct Settings {
  char name[64];
  uint8_t id;
  tm lastDateTimeSync;
  uint8_t alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_DATA_STORE];
};

void writeSetting(Settings* settings, int address = SETTINGS_ADDRESS, size_t length = sizeof(Settings));
void writeSetting(Settings* settings, int address, size_t length) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write((uint8_t)(address >> 8)); // MSB of address
  Wire.write((uint8_t)(address & 0xFF)); // LSB of address

  uint8_t* byteData = (uint8_t*)settings;
  for (size_t i = 0; i < length; i++) {
    Wire.write(byteData[i]);
  }

  Wire.endTransmission();
  vTaskDelay(10 / portTICK_PERIOD_MS); // Delay to allow EEPROM write cycle to complete
}

void readSettings(Settings* settings, int address = SETTINGS_ADDRESS, size_t length = sizeof(Settings));
void readSettings(Settings* settings, int address, size_t length) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write((uint8_t)(address >> 8)); // MSB of address
  Wire.write((uint8_t)(address & 0xFF)); // LSB of address
  Wire.endTransmission();

  Wire.requestFrom(EEPROM_ADDR, length);
  uint8_t* byteData = (uint8_t*)settings;
  if (Wire.available() >= length) {
    for (size_t i = 0; i < length; i++) {
      byteData[i] = Wire.read();
    }
  }
}