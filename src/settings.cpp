/**
 * @file         : settings.cpp
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

#include "settings.h"

void saveSettings(Settings* settings) {
  EEPROM.put(EEPROM_SETTINGS_ADDRESS, settings);
  EEPROM.commit();
}

void readSettings(Settings* settings) {
  EEPROM.get(EEPROM_SETTINGS_ADDRESS, settings);
}

void printI2cDevices(byte* devices) {
  byte error, address;
  int nDevices;
 
  TRACE("Scanning...\n");
 
  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0)
    {
      TRACE("I2C device found at address 0x");
      if (devices != NULL) {
        devices[nDevices] = address;
      }
      if (address<16)
        TRACE("0");
      PRINT(address, HEX);
      TRACE("  !\n");
      nDevices++;
    }
    else if (error==4)
    {
      TRACE("Unknown error at address 0x");
      if (address < 16)
        TRACE("0");
      PRINTLN(address, HEX);
    }    
  }
  if (nDevices == 0)
    TRACE("No I2C devices found\n");
  else
    TRACE("done\n");
}

String getAlarms(Settings settings) {
  String result = "[";

  for (int i = 0; i < SETTINGS_MAX_ALARMS; i++) {
    result += "[";
    for (int j = 0; j < SETTINGS_ALARM_STATES; j++) {
      result += "{";
      
        result += "  \"id\": " + String(settings.alarm[i][j].id) + ",\n";
        result += "  \"weekday\": " + String(settings.alarm[i][j].weekday) + ",\n";
        result += "  \"hour\": " + String(settings.alarm[i][j].hour) + ",\n";
        result += "  \"minute\": " + String(settings.alarm[i][j].minute) + ",\n";
        result += "  \"status\": " + String(settings.alarm[i][j].status) + "\n";

      result += "}";
      if (j < SETTINGS_ALARM_STATES - 1) {
        result += ",";
      }
    }
    result += "]";
    if (i < SETTINGS_MAX_ALARMS - 1) {
      result += ",";
    }
  }

  result += "]";

  return result;
}


// uint32_t calculateWateringDuration(uint8_t potSize) {
//   uint32_t targetMl = (((potSize * 1000) / 10 ) / 4); // one cuarter of 10% the size of the pot 
//   return targetMl / (WATER_PUMP_ML_PER_MINUTE / 60);
// }

uint32_t getTotalWateringTime(Settings settings) {
  uint32_t result = 0;
  for (int i = 0; i < SETTINGS_MAX_PLANTS; i++) {
    result += calculateWateringDuration(settings.plant[i].size);
  }
  return result;
}

String getPlants(Settings settings) {
  String result = "[";

  for (int i = 0; i < SETTINGS_MAX_PLANTS; i++) {
      result += "{";
      result += "  \"id\": " + String(settings.plant[i].id) + ",\n";
      result += "  \"size\": " + String(settings.plant[i].size) + ",\n";
      result += "  \"status\": " + String(settings.plant[i].status) + "\n";
      result += "}";
    if (i < SETTINGS_MAX_PLANTS - 1) {
      result += ",";
    }
  }
  result += "]";
  return result;
}

bool initSDCard() {
  if(!SD.begin(SS)) {
    TRACE("Card Mount Failed\n");
    return false;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE) {
    TRACE("No SD card attached\n");
    return false;
  }

  TRACE("SD Card Type: ");
  if(cardType == CARD_MMC) {
    TRACE("MMC\n");
  } else if(cardType == CARD_SD) {
    TRACE("SDSC\n");
  } else if(cardType == CARD_SDHC) {
    TRACE("SDHC\n");
  } else {
    TRACE("UNKNOW\n");
    return false;
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  uint64_t freeSize = cardSize - (SD.usedBytes() / (1024 * 1024));
  TRACE("SD Card Size: %lluMB\n", cardSize);
  TRACE("SD Free Size: %lluMB\n", freeSize);
  return true;
}

String listDirectory(const char* directory, unsigned long from, unsigned long to) {
  String fileList = "[";

  File root = SD.open(directory);
  if (root) {
    int count = 0;
    while (true) {
      File entry = root.openNextFile();
      if (!entry || count >= 1000) {
        break;
      }
      String fileName = entry.name();
      if (fileName.startsWith("log-") && fileName.endsWith(".csv")) {
        int timestamp = fileName.substring(4, fileName.length() - 4).toInt();
        if (timestamp >= from && timestamp <= to) {
          if (fileList != "[") {
            fileList += ",";
          }
          fileList += "\"" + fileName + "\"";
          count++;
        }
      }
      entry.close();
    }
    root.close();
  } else {
    TRACE("Failed to open directory\n");
  }

  fileList += "]";
  return fileList;
}

String listDirectory2(const char* directory) {
  String fileList = "[";

  File root = SD.open(directory);
  if (root) {
    while (true) {
      File entry = root.openNextFile();
      if (!entry) {
        break;
      }
      if (fileList != "[") {
        fileList += ",";
      }
      fileList += "\"" + String(entry.name()) + "\"";
      entry.close();
    }
    root.close();
  } else {
    TRACE("Failed to open directory\n");
  }

  fileList += "]";
  return fileList;
}

String listLogFiles(const char* directory, int from, int to) {
  String fileList = "[";

  File root = SD.open(directory);
  if (root) {
    int count = 0;
    while (true) {
      File entry = root.openNextFile();
      if (!entry || count >= 1000) {
        break;
      }
      String fileName = entry.name();
      if (fileName.startsWith("log-") && fileName.endsWith(".csv")) {
        int timestamp = fileName.substring(4, fileName.length() - 4).toInt();
        if (timestamp >= from && timestamp <= to) {
          if (fileList != "[") {
            fileList += ",";
          }
          fileList += "\"" + fileName + "\"";
          count++;
        }
      }
      entry.close();
    }
    root.close();
  } else {
    TRACE("Failed to open directory\n");
  }

  fileList += "]";
  return fileList;
}


bool saveLog(DateTime now, String name, int id, int milliliters, int duration, const char* destinationFolder) {
  if(!initSDCard()) {
    return false;
  }
  if (!createDirectoryIfNotExists(destinationFolder)) {
    return false;
  }
  if (!SD.exists(destinationFolder)) {
    TRACE("Creating directory: %s\n", destinationFolder);
    SD.mkdir(destinationFolder);
  }
  String fileName = String(destinationFolder) + "/log-" + String(now.unixtime()) + ".csv";
  File file = SD.open(fileName, FILE_WRITE);
  
  if (!file) {
    TRACE("Failed to open file for writing\n");
    return false;
  }
  
  file.printf("%s,%s,%s,%s\n", "id", "name", "milliliters", "duration");
  file.printf("%d,%s,%d,%d\n", id, name.c_str(), milliliters, duration);
  
  file.close();
  return true;
}

bool createDirectoryIfNotExists(const char* path) {
  if (!SD.exists(path)) {
    if (SD.mkdir(path)) {
      return true;
    } else {
      return false;
    }
  }
  return true;
}

unsigned long getLogCount(const char* destinationFolder) {
  File directory = SD.open(destinationFolder);
  unsigned long fileCount = 0;

  if (directory) {
    while (true) {
      File entry = directory.openNextFile();
      if (!entry) {
        break;
      }
      String fileName = entry.name();
      if (fileName.startsWith("log-") && fileName.endsWith(".csv")) {
        fileCount++;
      }
      entry.close();
    }
    directory.close();
  }
  return fileCount;
}

int getActiveAlarmId(Settings settings, DateTime now) {
  // Get the bitmask for the current day
  uint8_t currentDayMask = 1 << now.dayOfTheWeek(); // Assuming 0 is Sunday, 1 is Monday, ...
  // Iterate through all alarms
  for (uint8_t i = 0; i < SETTINGS_MAX_ALARMS; i++) {
    // Check if the alarm is active (alarm[alarmNumber][0] == 1)
    if (settings.alarm[i][0].status == 1 && settings.alarm[i][1].status == 1) {
      // Extract alarm time components
      uint8_t alarmId = settings.alarm[i][0].id;
      uint8_t alarmWeekday = settings.alarm[i][0].weekday;
      uint8_t startAlarmHour = settings.alarm[i][0].hour;
      uint8_t startAlarmMinute = settings.alarm[i][0].minute;
      uint8_t status = settings.alarm[i][0].status;

      uint8_t endAlarmHour = settings.alarm[i][1].hour;
      uint8_t endAlarmMinute = settings.alarm[i][1].minute;
      // uint8_t alarmSecond = settings.alarm[i][1][3];

      // Check if the alarm matches the current time
      if ((alarmWeekday & currentDayMask) != 0) {
        if(now.hour() >= startAlarmHour && now.hour() <= endAlarmHour) {
          if(now.minute() >= startAlarmMinute && now.minute() < endAlarmMinute) { 
            // Alarm is active
            if (status == 1) {
              return alarmId;
              break;
            }
          }
        }
      }
    }
  }
  return -1;
}

// Helper function to calculate seconds from hours, minutes, and seconds
uint32_t toSeconds(uint8_t hours, uint8_t minutes, uint8_t seconds) {
  return hours * 3600 + minutes * 60 + seconds;
}

int getNextAlarmId(Settings settings, DateTime now) {
  uint32_t minTimeToNextAlarm = UINT_MAX; // Initialize with maximum possible value
  int nextAlarmId = -1; // Initialize with -1 to indicate no alarm found
  uint32_t currentTimeInSeconds = toSeconds(now.hour(), now.minute(), now.second());
  uint8_t currentDayOfWeek = now.dayOfTheWeek();

  for (uint8_t i = 0; i < SETTINGS_MAX_ALARMS; i++) {
    if (settings.alarm[i][0].status == 1 && settings.alarm[i][1].status == 1) {
      uint8_t alarmWeekdayMask = settings.alarm[i][0].weekday;
      uint8_t startAlarmHour = settings.alarm[i][0].hour;
      uint8_t startAlarmMinute = settings.alarm[i][0].minute;
      uint8_t startAlarmSecond = 0; // Assuming seconds are not stored, default to 0

      uint32_t alarmTimeInSeconds = toSeconds(startAlarmHour, startAlarmMinute, startAlarmSecond);

      for (uint8_t dayOffset = 0; dayOffset < 7; dayOffset++) {
        uint8_t dayToCheck = (currentDayOfWeek + dayOffset) % 7;
        uint8_t dayMask = 1 << dayToCheck;

        if ((alarmWeekdayMask & dayMask) != 0) {
          uint32_t timeToNextAlarm;

          if (dayOffset == 0 && alarmTimeInSeconds > currentTimeInSeconds) {
            // Same day, future time
            timeToNextAlarm = alarmTimeInSeconds - currentTimeInSeconds;
          } else {
            // Future days
            timeToNextAlarm = (86400 * dayOffset) + alarmTimeInSeconds - currentTimeInSeconds;
          }

          if (timeToNextAlarm < minTimeToNextAlarm) {
            minTimeToNextAlarm = timeToNextAlarm;
            nextAlarmId = settings.alarm[i][0].id; // Update the next alarm ID
          }
        }
      }
    }
  }

  return nextAlarmId;
}

uint32_t getNextAlarmTime(Settings settings, DateTime now) {
  uint32_t minTimeToNextAlarm = UINT_MAX; // Initialize with maximum possible value
  uint32_t currentTimeInSeconds = toSeconds(now.hour(), now.minute(), now.second());
  uint8_t currentDayOfWeek = now.dayOfTheWeek();

  for (uint8_t i = 0; i < SETTINGS_MAX_ALARMS; i++) {
    if (settings.alarm[i][0].status == 1 && settings.alarm[i][1].status == 1) {
      uint8_t alarmWeekdayMask = settings.alarm[i][0].weekday;
      uint8_t startAlarmHour = settings.alarm[i][0].hour;
      uint8_t startAlarmMinute = settings.alarm[i][0].minute;
      uint8_t startAlarmSecond = 0; // Assuming seconds are not stored, default to 0

      uint32_t alarmTimeInSeconds = toSeconds(startAlarmHour, startAlarmMinute, startAlarmSecond);

      for (uint8_t dayOffset = 0; dayOffset < 7; dayOffset++) {
        uint8_t dayToCheck = (currentDayOfWeek + dayOffset) % 7;
        uint8_t dayMask = 1 << dayToCheck;

        if ((alarmWeekdayMask & dayMask) != 0) {
          uint32_t timeToNextAlarm;

          if (dayOffset == 0 && alarmTimeInSeconds > currentTimeInSeconds) {
            // Same day, future time
            timeToNextAlarm = alarmTimeInSeconds - currentTimeInSeconds;
          } else {
            // Future days
            timeToNextAlarm = (86400 * dayOffset) + alarmTimeInSeconds - currentTimeInSeconds;
          }

          if (timeToNextAlarm < minTimeToNextAlarm) {
            minTimeToNextAlarm = timeToNextAlarm;
          }
        }
      }
    }
  }

  return minTimeToNextAlarm;
}

bool isAlarmOn(Settings settings, DateTime now) {
  if (getActiveAlarmId(settings, now) > -1) {
    return true;
  }
  return false;
}

bool saveAlarms(JsonDocument json, Alarm alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES]) {
  JsonArray alarmArray = json["alarm"].as<JsonArray>();
  int numAlarms = alarmArray.size();
  if (numAlarms > SETTINGS_MAX_ALARMS) {
    TRACE("Exceeded maximum number of alarms\n");
    return false;
  }

  // Clean all alarms
  memset(alarm, 0, sizeof(Alarm) * SETTINGS_MAX_ALARMS * SETTINGS_ALARM_STATES);

  for (int alarmIndex = 0; alarmIndex < numAlarms; alarmIndex++) {
    JsonArray alarmData = alarmArray[alarmIndex].as<JsonArray>();
    if (alarmData.size() != SETTINGS_ALARM_STATES) {
      TRACE("Invalid alarm format\n");
      return false;
    }

    for (int i = 0; i < SETTINGS_ALARM_STATES; i++) {
      
      JsonObject alarmSetting = alarmData[i].as<JsonObject>();

      if (!alarmSetting.containsKey("weekday") || !alarmSetting.containsKey("hour") || !alarmSetting.containsKey("minute") || !alarmSetting.containsKey("status")) {
        TRACE("Invalid alarm format\n");
        return false;
      }

      uint8_t id = alarmSetting["id"];
      uint8_t weekday = alarmSetting["weekday"];
      uint8_t hour = alarmSetting["hour"];
      uint8_t minute = alarmSetting["minute"];
      uint8_t status = alarmSetting["status"];

      // Validate hour, minute, and active values
      if (hour > 23 || minute > 59) {
        TRACE("Invalid alarm settings\n");
        return false;
      }

      // Store the alarm settings
      alarm[alarmIndex][i].id = id;
      alarm[alarmIndex][i].weekday = weekday;
      alarm[alarmIndex][i].hour = hour;
      alarm[alarmIndex][i].minute = minute;
      alarm[alarmIndex][i].status = status;
    }
  }
  return true;
}

bool savePlants(JsonDocument json, Plant plants[SETTINGS_MAX_PLANTS]) {
  JsonArray plantArray = json["plants"].as<JsonArray>();
  int numPlant = plantArray.size();
  if (numPlant > SETTINGS_MAX_PLANTS) {
    TRACE("Exceeded maximum number of plants\n");
    return false;
  }

  // Clean all alarms
  memset(plants, 0, sizeof(Plant) * SETTINGS_MAX_PLANTS);

  for (int plantIndex = 0; plantIndex < numPlant; plantIndex++) {
    JsonObject plantData = plantArray[plantIndex].as<JsonObject>();

    // Validate that all required fields are present
    if (!plantData.containsKey("id") || !plantData.containsKey("size") || !plantData.containsKey("status")) {
      TRACE("Missing required plant fields\n");
      return false;
    }

    // Extract values from JSON object
    uint8_t id = plantData["id"];
    uint8_t size = plantData["size"];
    uint8_t status = plantData["status"];

    // Validate hour, minute, and active values
    if (id < 0 || size < 0) {
      TRACE("Invalid plant settings\n");
      return false;
    }

    // Store the plant settings
    plants[plantIndex].id = id;
    plants[plantIndex].size = size;
    plants[plantIndex].status = status;
  }
  return true;
}

void beep(uint8_t times, unsigned long delay) {
  TRACE("beeping times: %d delay: %d\n", times, delay);
  pinMode(BUZZER_PIN, OUTPUT);
  for(uint8_t i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    vTaskDelay(delay / portTICK_PERIOD_MS);
    digitalWrite(BUZZER_PIN, LOW);
    vTaskDelay(delay / portTICK_PERIOD_MS);
  }
}

void turnOnPin(Adafruit_MCP23X17 mcp, int pinNumber) {
  if (pinNumber >= 0 && pinNumber < I2C_MCP_PINCOUNT) {
    for (int i = 0; i < 16; i++) {
      if (i == pinNumber) {
        mcp.digitalWrite(i, LOW); // Turn on the specified pin
      } else {
        mcp.digitalWrite(i, HIGH); // Turn off all other pins
      }
    }
  }
}

JsonDocument readConfig() {
  JsonDocument doc;
  if(!initSDCard()) {
    return doc;
  };

  File configFile = SD.open("/config.json");

  if (!configFile) {
    TRACE("Failed to open config file\n");
    return doc;
  }

  // Read the file content into a string
  String jsonString = "";
  while (configFile.available()) {
    jsonString += configFile.readString();
  }
  configFile.close();

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    TRACE("deserializeJson() failed: %s\n", error.c_str());
    return doc;
  }

  return doc;
}

String scanWifiNetworks() {
  int n = WiFi.scanNetworks();
  String hotspots ="[\n";
  if (n == 0) {
    TRACE("No networks found.\n");
  } else {
    // Allocate memory for SSID array
    TRACE("%d networks found: \n", n);
    for (int i = 0; i < n; ++i) {
      TRACE("%d: %s (%d dBm) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "open" : "encrypted");
      hotspots += "  {\n";
      hotspots += "    \"ssid\": \"" + WiFi.SSID(i) + "\",\n";
      hotspots += "    \"rssi\": " + String(WiFi.RSSI(i)) + ",\n";
      hotspots += "    \"encryptionType\": " + String(WiFi.encryptionType(i)) + "\n";
      if ((i + 1) < n) {
        hotspots += "  },\n";
      } else {
        hotspots += "  }\n";
      }
      vTaskDelay(75 / portTICK_PERIOD_MS);
    }
  }
  hotspots += "]";
  return hotspots;
}

const char* getResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  const char* result;
  switch (reason) {
    case ESP_RST_POWERON:
      result = "Power on reset";
      break;
    case ESP_RST_EXT:
      result = "External reset";
      break;
    case ESP_RST_SW:
      result = "Software reset";
      break;
    case ESP_RST_PANIC:
      result = "Panic reset";
      break;
    case ESP_RST_INT_WDT:
      result = "Interrupt watchdog reset";
      break;
    case ESP_RST_TASK_WDT:
      result = "Task watchdog reset";
      break;
    case ESP_RST_WDT:
      result = "Other watchdog reset";
      break;
    case ESP_RST_DEEPSLEEP:
      result = "Deep sleep reset";
      break;
    case ESP_RST_BROWNOUT:
      result = "Brownout reset";
      break;
    case ESP_RST_SDIO:
      result = "SDIO reset";
      break;
    default:
      result = "Unknown reset reason";
      break;
  }
  return result;
}

String uptimeStr() {
  // Get the uptime in milliseconds
  unsigned long millisec = millis();

  // Calculate hours, minutes, and seconds
  unsigned long totalSeconds = millisec / 1000;
  unsigned long hours = totalSeconds / 3600;
  unsigned long minutes = (totalSeconds % 3600) / 60;
  unsigned long seconds = totalSeconds % 60;

  // Format the uptime as "hh:mm:ss"
  char uptime[9]; // Buffer to hold the formatted string
  snprintf(uptime, sizeof(uptime), "%02lu:%02lu:%02lu", hours, minutes, seconds);

  return String(uptime);
}

// Function to add time interval to the current time
String addTimeInterval(uint32_t seconds, DateTime now) {
  // Add the total seconds to the current time
  time_t futureTime;
  TRACE("unixtime: %lu\n", now.unixtime());
  TRACE("seconds: %lu\n", seconds);
  TRACE("FutureTimeSec: %lu\n", futureTime);

  // If using NTP Time
  // tm timeinfo;
  // if(!getLocalTime(&timeinfo)){
  //   TRACE("Failed to obtain time\n");
  // }

  // futureTime = mktime(&timeinfo) + seconds;


  futureTime = now.unixtime() + seconds;

  // Convert to Unix time
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&futureTime));
  TRACE("FutureTime: %s\n", buffer);

  return String(buffer);
}

String settingsToJson(const Settings& settings) {
  // Create a JSON document with an estimated size
  JsonDocument doc;

  // Fill the JSON document
  doc["id"] = settings.id;
  doc["hostname"] = settings.hostname;
  doc["lastDateTimeSync"] = settings.lastDateTimeSync;
  doc["updatedOn"] = settings.updatedOn;
  doc["rebootOnWifiFail"] = settings.rebootOnWifiFail;
  doc["flowCalibrationFactor"] = settings.flowCalibrationFactor;
  
  JsonArray alarms = doc["alarms"].to<JsonArray>();
  for (uint8_t i = 0; i < SETTINGS_MAX_ALARMS; i++) {
    JsonArray alarmStates = alarms.add<JsonArray>();
    for (uint8_t j = 0; j < SETTINGS_ALARM_STATES; j++) {
      JsonObject alarmState = alarmStates.add<JsonObject>();
      alarmState["weekday"] = settings.alarm[i][j].weekday;
      alarmState["hour"] = settings.alarm[i][j].hour;
      alarmState["minute"] = settings.alarm[i][j].minute;
      alarmState["status"] = settings.alarm[i][j].status;
    }
  }

  doc["maxPlants"] = settings.maxPlants;
  
  JsonArray plants = doc["plants"].to<JsonArray>();
  for (uint8_t i = 0; i < settings.maxPlants; i++) {
    JsonObject plant = plants.add<JsonObject>();
    plant["id"] = settings.plant[i].id;
    plant["size"] = settings.plant[i].size;
    plant["status"] = settings.plant[i].status;
  }

  doc["hasDisplay"] = settings.hasDisplay;
  doc["hasRTC"] = settings.hasRTC;
  doc["hasEEPROM"] = settings.hasEEPROM;
  doc["hasMCP"] = settings.hasMCP;

  // Serialize the JSON document to a string
  String jsonString;
  serializeJson(doc, jsonString);

  return jsonString;
}
