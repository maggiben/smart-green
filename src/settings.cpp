#include "settings.h"

void saveSettings(Settings* settings) {
  EEPROM.put(SETTINGS_ADDRESS, settings);
  EEPROM.commit();
}

void readSettings(Settings* settings) {
  EEPROM.get(SETTINGS_ADDRESS, settings);
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
      
        result += "  \"weekday\": " + String(settings.alarm[i][j].weekday) + ",\n";
        result += "  \"hour\": " + String(settings.alarm[i][j].hour) + ",\n";
        result += "  \"minute\": " + String(settings.alarm[i][j].minute) + ",\n";
        result += "  \"status\": " + String(settings.alarm[i][j].status) + "\n";
      // for (int k = 0; k < SETTINGS_ALARM_DATA_STORE; k++) {
      //   result += String(settings.alarm[i][j][k]);
      //   if (k < SETTINGS_ALARM_DATA_STORE - 1) {
      //     result += ",";
      //   }
      // }

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

bool initSDCard() {
  // pinMode(SS, OUTPUT);
  // digitalWrite(SS, HIGH); // Set SS pin high initially
  pinMode(BUZZER_PIN, OUTPUT);
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

String listRootDirectory() {
  String fileList = "[";

  File root = SD.open("/");
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

bool saveLog(DateTime now, String name, int id, int milliliters, int duration) {
  String fileName = "/log-" + String(now.unixtime()) + ".csv";
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

bool isAlarmOn(Settings settings, DateTime now) {
  // Get the bitmask for the current day
  uint8_t currentDayMask = 1 << now.dayOfTheWeek(); // Assuming 0 is Sunday, 1 is Monday, ...
  // Iterate through all alarms
  for (int i = 0; i < SETTINGS_MAX_ALARMS; i++) {
    // Check if the alarm is active (alarm[alarmNumber][0] == 1)
    if (settings.alarm[i][0].status == 1 && settings.alarm[i][1].status == 1) {
      // Extract alarm time components
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
              return true;
              break;
            }
          }
        }
      }
    }
  }
  // No active alarm found
  return false;
}

bool setupAlarms(WebServer &server, Alarm alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES]) {
  JsonDocument json;
  DeserializationError error = deserializeJson(json, server.arg("plain"));

  if (error) {
    TRACE("deserializeJson() failed:\n");
    TRACE(error.c_str());
    return false;
  }

  JsonArray alarmArray = json["alarm"].as<JsonArray>();
  int numAlarms = alarmArray.size();
  if (numAlarms > SETTINGS_MAX_ALARMS) {
    TRACE("Exceeded maximum number of alarms\n");
    return false;
  }

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
      alarm[alarmIndex][i].weekday = weekday;
      alarm[alarmIndex][i].hour = hour;
      alarm[alarmIndex][i].minute = minute;
      alarm[alarmIndex][i].status = status;

      // JsonArray alarmSetting = alarmData[i].as<JsonArray>();
      // if (alarmSetting.size() != SETTINGS_ALARM_DATA_STORE) {
      //   TRACE("Invalid alarm format\n");
      //   return false;
      // }

      // uint8_t weekday = alarmSetting[0];
      // uint8_t hour = alarmSetting[1];
      // uint8_t minute = alarmSetting[2];
      // uint8_t status = alarmSetting[3];

      // // Validate hour, minute, and active values
      // if (hour > 23 || minute > 59 || status > 1) {
      //   TRACE("Invalid alarm settings\n");
      //   return false;
      // }

      // return false;
      // // Store the alarm settings
      // alarm[alarmIndex][i].weekday = weekday;
      // alarm[alarmIndex][i].hour = hour;
      // alarm[alarmIndex][i].minute = minute;
      // alarm[alarmIndex][i].status = status;
    }
  }
  return true;
}

void setupPlants(WebServer &server, uint8_t plants[SETTINGS_MAX_PLANTS][SETTINGS_PLANTS_STORE]) {
  JsonDocument json;
  DeserializationError error = deserializeJson(json, server.arg("plain"));

  if (error) {
    TRACE("deserializeJson() failed:\n");
    TRACE(error.c_str());
    return;
  }

  JsonArray alarmArray = json["plants"].as<JsonArray>();
  int numPlant = alarmArray.size();
  if (numPlant > SETTINGS_MAX_PLANTS) {
    TRACE("Exceeded maximum number of plants\n");
    return;
  }

  for (int plantIndex = 0; plantIndex < numPlant; plantIndex++) {
    JsonArray plantData = alarmArray[plantIndex].as<JsonArray>();
    if (plantData.size() != SETTINGS_PLANTS_STORE) {
      TRACE("Invalid alarm format\n");
      return;
    }
  
    /* "id", "pot size" and "status" */
    uint8_t id = plantData[0];
    uint8_t size = plantData[1];
    uint8_t status = plantData[2];

    // Validate hour, minute, and active values
    if (id < 0 || size < 0) {
      TRACE("Invalid alarm settings\n");
      return;
    }

    // Store the plant settings
    plants[plantIndex][0] = id;
    plants[plantIndex][1] = size;
    plants[plantIndex][2] = status;
  }
}

void beep(uint8_t times) {
  TRACE("beeping times: %d\n", times);
  pinMode(BUZZER_PIN, OUTPUT);
  for(uint8_t i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    digitalWrite(BUZZER_PIN, LOW);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

String getI2cDeviceList() {
  String result = "[";
  byte* i2cDevices = (byte*)malloc(sizeof(byte) * MAX_I2C_DEVICES);
  printI2cDevices(i2cDevices);
  for (int i = 0; i < sizeof(i2cDevices) / sizeof(i2cDevices[0]); i++) {
    result += String(i2cDevices[i]);
    if (i < sizeof(i2cDevices) / sizeof(i2cDevices[0]) - 1) {
      result += ", ";
    }
  }
  result += "]";
  free(i2cDevices);
  return result;
}

bool isConnected() {
  return (WiFi.status() == WL_CONNECTED);
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

void handleWifiConnectionError(String error, Settings settings, bool restart) {
  TRACE("Error: %s\n", error.c_str());
  if (settings.rebootOnWifiFail) {
    TRACE("Rebooting now...\n");
    vTaskDelay(150 / portTICK_PERIOD_MS);
    ESP.restart();
  }
}
