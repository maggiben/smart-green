#pragma once
#include <RTClib.h> // Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include <EEPROM.h>
#include <SD.h>
#include <Adafruit_MCP23X17.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "constants.h"

#define SETTINGS_ADDRESS                  0       /* Active plants (battery backed ram address) */
#define HOSTNAME_MAX_LENGTH               64      /* Max hostname length */
#define SETTINGS_MAX_ALARMS               8       /* Max amount of settable alarms */
#define SETTINGS_ALARM_STATES             2       /* 2 states on and off */
#define SETTINGS_ALARM_DATA_STORE         4       /* Alarm store array: "weekday", "hour" and "minute" */
#define SETTINGS_MAX_PLANTS               10      /* Maximun amount of allowed plants */
#define SETTINGS_PLANTS_STORE             3       /* Plant store array: "id", "pot size" and "status" */
#define SETTINGS_REBOOT_ON_WIFIFAIL       false   /* Reset if wifi fails 0 = false 1 = true */

struct Alarm {
  uint8_t weekday;
  uint8_t hour;
  uint8_t minute;
  uint8_t status;
};

struct Settings {
  // char * name;
  char hostname[HOSTNAME_MAX_LENGTH];
  uint8_t id;
  uint32_t lastDateTimeSync;
  uint32_t updatedOn;
  boolean rebootOnWifiFail;
  uint8_t flowCalibrationFactor;
  Alarm alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES];
  /* In case you need to set the values statically use these: */
  // uint8_t alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES][SETTINGS_ALARM_DATA_STORE] = {
  //  {
  //    { 0b00000001, 14, 30, 0 }, 
  //    { 0b00000001, 14, 30, 0 },
  //  },
  // };
  uint8_t maxPlants;
  uint8_t plants[SETTINGS_MAX_PLANTS][SETTINGS_PLANTS_STORE];
  bool useDisplay;
};

void printI2cDevices(byte* devices = NULL);
String getI2cDeviceList();
String getAlarms(Settings settings);
int getActiveAlarmId(Settings settings, DateTime now);
bool isAlarmOn(Settings settings, DateTime now);
void beep(uint8_t times);
bool setupAlarms(WebServer &server, Alarm alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES]);
bool getAlarmRunningState();
void setAlarmRunningState(bool state);
void toggleAlarmRunningState();
String listRootDirectory();
bool initSDCard();
bool saveLog(DateTime now, String name, int id, int milliliters, int duration);
bool isConnected();
void turnOnPin(Adafruit_MCP23X17 mcp, int pinNumber);
void handleWifiConnectionError(String error, Settings settings, bool restart = false);