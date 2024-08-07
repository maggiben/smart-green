/**
 * @file         : settings.h
 * @summary      : Smart Indoor Automation System
 * @version      : 1.0.0
 * @project      : smart-green
 * @description  : A Smart Indoor Automation System
 * @author       : Benjamin Maggi
 * @email        : benjaminmaggi@gmail.com
 * @date         : 23 Apr 2024
 * @license:     : MIT
 *
 * Copyright 2021 Benjamin Maggi <benjaminmaggi@gmail.com>
 *
 *
 * License:
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **/

#pragma once
#include <RTClib.h> // Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include <EEPROM.h>
#include <SD.h>
#include <Adafruit_MCP23X17.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "constants.h"

#define EEPROM_SETTINGS_ADDRESS           0       /* Active plants (battery backed ram address) */
#define HOSTNAME_MAX_LENGTH               64      /* Max hostname length */
#define SETTINGS_MAX_ALARMS               8       /* Max amount of settable alarms */
#define SETTINGS_ALARM_STATES             2       /* 2 states on and off */
#define SETTINGS_MAX_PLANTS               11      /* Maximun amount of allowed plants & valves */
#define SETTINGS_REBOOT_ON_WIFIFAIL       false   /* Reset if wifi fails 0 = false 1 = true */

struct Alarm {
  uint8_t id;
  uint8_t weekday;
  uint8_t hour;
  uint8_t minute;
  uint8_t status;
};

struct Plant {
  uint8_t id;
  uint8_t size;
  uint8_t status;
};


struct TaskLog {
  Alarm alarm;
  uint32_t updatedOn;
  uint32_t flow[SETTINGS_MAX_PLANTS]; 
  uint8_t lastExecutionId;
  uint8_t nextExecutionId;
};

struct Settings {
  // char * name;
  char hostname[HOSTNAME_MAX_LENGTH];
  uint8_t id;
  uint32_t lastDateTimeSync;
  uint32_t updatedOn;
  bool rebootOnWifiFail;
  uint8_t flowCalibrationFactor;
  Alarm alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES];
  uint8_t maxPlants;
  Plant plant[SETTINGS_MAX_PLANTS];
  TaskLog taskLog;
  bool hasDisplay;
  bool hasRTC;
  bool hasEEPROM;
  bool hasMCP;
};

struct Network {
  // Variables to hold the SSID and password
  String ssid;
  String password;
};

struct Config {
  Network network;
};

/**
 * Serialization functions
 */
String getAlarms(Settings settings);
String getPlants(Settings settings);
String uptimeStr();
String scanWifiNetworks();
String addTimeInterval(uint32_t seconds, DateTime now);
String settingsToJson(const Settings& settings);
String listDirectory(const char* directory = "/logs", unsigned long from = 0, unsigned long to = 1024);
String listDirectory2(const char* directory);

/**
 * Debugging
 */
void printI2cDevices(byte* devices = NULL);
void beep(uint8_t times, unsigned long delay = 500);
const char* getResetReason();

/**
 * Alarm functions
 */
bool saveAlarms(JsonDocument json, Alarm alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES]);
int getActiveAlarmId(Settings settings, DateTime now);
bool isAlarmOn(Settings settings, DateTime now);
int getNextAlarmId(Settings settings, DateTime now);

/**
 * File management
 */
bool initSDCard();
JsonDocument readConfig();
bool createDirectoryIfNotExists(const char* path);
bool saveLog(DateTime now, String name, int id, int milliliters, int duration, const char* destinationFolder = "/logs");
unsigned long getLogCount(const char* destinationFolder = "/logs");


void turnOnPin(Adafruit_MCP23X17 mcp, int pinNumber);

/**
 * Plant functions
 */
bool savePlants(JsonDocument json, Plant plants[SETTINGS_MAX_PLANTS]);
uint32_t calculateWateringDuration(uint8_t potSize);
uint32_t getTotalWateringTime(Settings settings);



uint32_t getNextAlarmTime(Settings settings, DateTime now);
uint32_t toSeconds(uint8_t hours, uint8_t minutes, uint8_t seconds);
bool setRTCFromISODate(String isoDate, RTC_DS3231 rtc);

