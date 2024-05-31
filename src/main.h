/**
 * @file         : main.h
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
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <Wire.h>
#include <FS.h>
#include <SD.h>
#include <EEPROM.h>
#include <time.h>
#include <RTClib.h> // Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP23X17.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "constants.h"
#include "settings.h"

// Settings
Settings settings = {
  // Assuming HOSTNAME is defined
  HOSTNAME,
  // Assuming id is 0
  0,
  // Assuming lastDateTimeSync is 0
  0,
  // Assuming updatedOn is 0
  0,
  // Assuming reboot on wifi failed is false
  SETTINGS_REBOOT_ON_WIFIFAIL,
  // Flow calibration value
  FLOW_CALIBRATION_FACTOR,
  // Initializing alarms all to 0 (disabled)
  {{{0}}},
  // Max Plants
  SETTINGS_MAX_PLANTS,
  // Plant settings
  {{0}},
  // Attemp to use Graphical Display
  USE_DISPLAY,
  USE_RTC,
  USE_EEPROM,
  USE_MCP
};

// i2c Clock
RTC_DS3231 rtc; // Address 0x68

// i2c Display
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire); // Address 0x3C

// i2c Port extender
Adafruit_MCP23X17 mcp; // Address 0x20

// Device IP Address
IPAddress ip;

// Need a WebServer for http access on port 80.
WebServer server(80);

// Need a WebServer for http access on port 80.
#ifndef server
  #define SERVER_RESPONSE_OK(...)  server.send(200, "application/jsont; charset=utf-8", __VA_ARGS__)
  #define SERVER_RESPONSE_SUCCESS()  SERVER_RESPONSE_OK("{\"success\":true}")
  // #define SERVER_RESPONSE_ERROR(code, ...)  server.send(code, "application/jsont; charset=utf-8", __VA_ARGS__)
  #define SERVER_RESPONSE_ERROR(code, error)  server.send(code, "application/json; charset=utf-8", String("{\"error\":\"") + error + "\"}")
#endif

#ifndef Task0
  TaskHandle_t Task0;
#endif
#ifndef Task1
  TaskHandle_t Task1;
#endif

#ifndef IS_ALARM_ON
  volatile bool IS_ALARM_ON = false;
#endif

#ifndef ENABLE_FLOW
  volatile byte FLOW_METER_PULSE_COUNT                  = 0;
  volatile unsigned long FLOW_METER_TOTAL_PULSE_COUNT   = 0;
  unsigned long OLD_INT_TIME                            = 0;
  unsigned long START_INT_TIME                          = 0;
  float FLOW_RATE                                       = 0.0;
  unsigned int FLOW_MILLILITRES                         = 0;
  unsigned long TOTAL_MILLILITRES                       = 0;
  uint8_t FLOW_SENSOR_STATE                             = HIGH;
#endif

#ifndef DISPLAY_INFO
  #define DISPLAY_INFO
  byte DISPLAY_INFO_DATA = 0;
#endif

bool setupMcp();
void pulseCounter();
void calcFlow();
void displayFlow();
void handleTestFlow();

void beep(uint8_t uint8_t);
// void errorMsg(String error, bool restart = true);
bool isConnected();
bool connectToWiFi(const char* ssid, const char* password, int max_tries = 20, int pause = 500);
void syncRTC();
void setTimezone(String timezone);
void initTime(String timezone);
long int getRtcOffset();
void printLocalTime();
void writeToEEPROM(int address, void* data, size_t length);
void readFromEEPROM(int address, void* data, size_t length);

// This function is called when the sysInfo service was requested.
void handleSystemInfo();
void handleValve();
void handleSaveSettings();
void handlePump();
void handleAlarm();
void handlePlants();
void handleLogs();
void handleRoot();
void displayTime();
void pumpWater(void *parameter);
void waterPlants();
void waterPlant(uint8_t valve, unsigned int duration, unsigned long millilitres);