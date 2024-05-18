/**
 * @file         : constants.h
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
// WiFi Credentials
#define WIFI_SSID                   "TP-Link_42B4"
#define WIFI_PASSWORD               "pirulo123"

#define HOSTNAME                    "indoor"
#define TIMEZONE                    "ART3ARST,M10.1.0/0,M3.3.0/0" // https://gist.github.com/tivaliy/a5ef9c7ccb4d57bdc248f0855c5eb1ff
#define MAX_I2C_DEVICES             128
#define I2C_MCP_PINCOUNT            12
#define EEPROM_ADDRESS              0x57
#define EEPROM_SIZE                 4096
#define EEPROM_SETTINGS_ADDRESS     0
// TRACE output simplified, can be deactivated here
#define TRACE(...)                  Serial.printf(__VA_ARGS__)
#define PRINT(...)                  Serial.print(__VA_ARGS__)
#define PRINTLN(...)                Serial.println(__VA_ARGS__)
#define JSONBOOL(value)             value ? F("true") : F("false")
#define BUZZER_PIN                  32
#define PUMP1_PIN                   10
#define PUMP2_PIN                   11
#define FLOW_METER_PIN              33
#define FLOW_METER_INTERRUPT        06
#define FLOW_CALIBRATION_FACTOR     220
#define USE_DISPLAY                 true
#define USE_RTC                     true
#define USE_EEPROM                  true
#define USE_MCP                     true
#define DISPLAY_ADDRESSS            0x3C
