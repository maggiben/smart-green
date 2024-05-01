#include "settings.h"

void saveSettings(Settings* settings) {
  uint16_t addr = 0;
  EEPROM.put(addr, settings);
  EEPROM.commit();
}

void readSettings(Settings* settings) {
  uint16_t addr = 0;
  EEPROM.get(addr, settings);
}

void printI2cDevices() {
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
      if (address<16)
        TRACE("0");
      PRINT(address, HEX);
      PRINTLN("  !");
 
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

String printAlarm(Settings settings) {
  String result = "[";

  for (int i = 0; i < SETTINGS_MAX_ALARMS; i++) {
    result += "[";
    for (int j = 0; j < SETTINGS_ALARM_STATES; j++) {
      result += "[";
      for (int k = 0; k < SETTINGS_ALARM_DATA_STORE; k++) {
        result += String(settings.alarm[i][j][k]);
        if (k < SETTINGS_ALARM_DATA_STORE - 1) {
          result += ",";
        }
      }
      result += "]";
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

bool isAlarmOn(Settings settings, DateTime now) {
  // Get the bitmask for the current day
  uint8_t currentDayMask = 1 << now.dayOfTheWeek(); // Assuming 0 is Sunday, 1 is Monday, ...
  // Iterate through all alarms
  for (int i = 0; i < SETTINGS_MAX_ALARMS; i++) {
    // Check if the alarm is active (alarm[alarmNumber][0] == 1)
    if (settings.alarm[i][0][3] == 1 && settings.alarm[i][1][3] == 1) {
      // Extract alarm time components
      uint8_t alarmWeekday = settings.alarm[i][0][0];
      
      uint8_t startAlarmHour = settings.alarm[i][0][1];
      uint8_t startAlarmMinute = settings.alarm[i][0][2];

      uint8_t endAlarmHour = settings.alarm[i][1][1];
      uint8_t endAlarmMinute = settings.alarm[i][1][2];
      // uint8_t alarmSecond = settings.alarm[i][1][3];

      // Check if the alarm matches the current time
      if ((alarmWeekday & currentDayMask) != 0) {
        if(now.hour() >= startAlarmHour && now.hour() <= endAlarmHour) {
          if(now.minute() >= startAlarmMinute && now.minute() < endAlarmMinute) { 
            // Alarm is active
            return true;
            break;
          }
        }
      }
    }
  }
  // No active alarm found
  return false;
}

void setupAlarms(WebServer &server, uint8_t alarm[SETTINGS_MAX_ALARMS][SETTINGS_ALARM_STATES][SETTINGS_ALARM_DATA_STORE]) {
  JsonDocument json;
  DeserializationError error = deserializeJson(json, server.arg("plain"));

  if (error) {
    TRACE("deserializeJson() failed:\n");
    TRACE(error.c_str());
    return;
  }

  JsonArray alarmArray = json["alarm"].as<JsonArray>();
  int numAlarms = alarmArray.size();
  if (numAlarms > SETTINGS_MAX_ALARMS) {
    TRACE("Exceeded maximum number of alarms\n");
    return;
  }

  for (int alarmIndex = 0; alarmIndex < numAlarms; alarmIndex++) {
    JsonArray alarmData = alarmArray[alarmIndex].as<JsonArray>();
    if (alarmData.size() != SETTINGS_ALARM_STATES) {
      TRACE("Invalid alarm format\n");
      return;
    }

    for (int i = 0; i < SETTINGS_ALARM_STATES; i++) {
      JsonArray alarmSetting = alarmData[i].as<JsonArray>();
      if (alarmSetting.size() != SETTINGS_ALARM_DATA_STORE) {
        TRACE("Invalid alarm format\n");
        return;
      }

      uint8_t weekday = alarmSetting[0];
      uint8_t hour = alarmSetting[1];
      uint8_t minute = alarmSetting[2];
      uint8_t active = alarmSetting[3];

      // Validate hour, minute, and active values
      if (hour > 23 || minute > 59 || active > 1) {
        TRACE("Invalid alarm settings\n");
        return;
      }

      // Store the alarm settings
      alarm[alarmIndex][i][0] = weekday;
      alarm[alarmIndex][i][1] = hour;
      alarm[alarmIndex][i][2] = minute;
      alarm[alarmIndex][i][3] = active;
    }
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