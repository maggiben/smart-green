#pragma once
#include <RTClib.h> // Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include <EEPROM.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "constants.h"

#define SETTINGS_ADDRESS 0            /* Active plants (battery backed ram address) */
#define HOSTNAME_MAX_LENGTH 64        /* Max hostname length */
#define SETTINGS_MAX_ALARMS 8         /* Max amount of settable alarms */
#define SETTINGS_ALARM_STATES 2       /* 2 states on and off */
#define SETTINGS_ALARM_DATA_STORE 4
#define SETTINGS_MAX_PLANTS 10        /* Maximun amount of allowed plants */

struct Settings {
  // char * name;
  char hostname[HOSTNAME_MAX_LENGTH];
  uint8_t id;
  uint32_t lastDateTimeSync;
  uint32_t updatedOn;
  boolean rebootOnWifiFail;
  float flowCalibrationFactor;
  uint8_t alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES][SETTINGS_ALARM_DATA_STORE];
  // uint8_t alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES][SETTINGS_ALARM_DATA_STORE] = {
  // {
  //   { 0b00000001, 14, 30, 0 }, 
  //   { 0b00000001, 14, 30, 0 },
  // }, {
  //   { 0b00000010, 14, 30, 0 },
  //   { 0b00000010, 14, 30, 0 },
  // }, {
  //   { 0b00000100, 14, 30, 0 },
  //   { 0b00000100, 14, 30, 0 },
  // }, {
  //   { 0b00001000, 14, 30, 0 },
  //   { 0b00001000, 14, 30, 0 },
  // }, {
  //   { 0b00010000, 14, 30, 0 },
  //   { 0b00010000, 14, 30, 0 },
  // }, {
  //   { 0b00100000, 14, 30, 0 },
  //   { 0b000100000, 14, 30, 0 },
  // }, {
  //   { 0b01000000, 14, 30, 0 },
  //   { 0b01001000, 14, 30, 0 },
  // }
  // };
};

void printI2cDevices();
String printAlarm(Settings settings);
bool isAlarmOn(Settings settings, DateTime now);
void beep(uint8_t times);
void setupAlarms(WebServer &server, uint8_t alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES][SETTINGS_ALARM_DATA_STORE]);
bool getAlarmRunningState();
void setAlarmRunningState(bool state);
void toggleAlarmRunningState();
