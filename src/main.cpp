/**
 * @file         : main.cpp
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

#include "main.h"

/**
 * Hardware setup
 */
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  while (!Serial) {
    vTaskDelay(100 / portTICK_PERIOD_MS); // wait for serial port to connect. Needed for native USB
  }

  Wire.begin();
  i2cMutex = xSemaphoreCreateMutex();
  if (i2cMutex == NULL) {
    TRACE("Error insufficient heap memory to create i2cMutex mutex\n");
  }

  // Check if EEPROM is ready
  Wire.beginTransmission(EEPROM_ADDRESS);
  if (Wire.endTransmission() != 0) {
    settings.hasEEPROM = false;
    beep(3, 250);
    TRACE("EEPROM not found or not ready\n");
    ESP.restart();
  } else {
    // Init EEPROM chip in the RTC module
    if (EEPROM.begin(EEPROM_SIZE)) {
      // EEPROM.put(EEPROM_SETTINGS_ADDRESS, settings);
      // EEPROM.commit();
      // vTaskDelay(500 / portTICK_PERIOD_MS);
      // beep(5, 150);
      // Read EEPROM settings
      EEPROM.get(EEPROM_SETTINGS_ADDRESS, settings);
      // TRACE("Settings: %s now: %d\n", settings.hostname, rtc.now().unixtime());
    } else {
      settings.hasEEPROM = false;
      beep(3, 250);
      TRACE("EEPROM not Working\n");
      ESP.restart();
    };
  }

  if (!rtc.begin()) {
    TRACE("Couldn't find RTC\n");
    beep(2, 250);
    settings.hasRTC = false;
    // Serial.flush();
    // abort();
  } else if (rtc.lostPower()) {
    TRACE("RTC lost power, let's set the time!\n");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    beep(2);
  }

  // Setup i2c port extender
  
  if(!setupMcp()) {
    settings.hasMCP = false;
    TRACE("MCP not Working\n");
  }

  printI2cDevices();

  if(!initSDCard()) {
    TRACE("SD not working\n");
    beep(4, 150);
  }

  JsonDocument config = readConfig();

  if (!config["updatedOn"].isNull()) { // Math.floor(Date.now() / 1000) & 0xFFFFFFFF
    // Config is newer
    uint32_t updatedOn = config["updatedOn"];
    TRACE("rtc update: %u\n", rtc.now().unixtime());
    TRACE("new update: %u\n", updatedOn);
    TRACE("old update: %u\n", settings.updatedOn);
    if(settings.updatedOn < updatedOn) {
      TRACE("Flashing new config!\n");
      beep(4, 50);
      savePlants(config, settings.plant);
      saveAlarms(config, settings.alarm);
      settings.updatedOn = updatedOn;
      EEPROM.put(EEPROM_SETTINGS_ADDRESS, settings);
      EEPROM.commit();
    }
  } else {
    TRACE("updatedOn is Null!\n");
  }

  if(!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESSS)) {; // Address 0x3C for 128x32
    TRACE("Display not working\n");
    settings.hasDisplay = false;
  } else {
    // Clear display
    TRACE("Display ok!\n");
    display.clearDisplay();
    display.display();
  }

  TRACE("settings.hostname %s\n", settings.hostname);

  // Create a queue capable of holding 10 strings of up to 100 characters each
  wateringStatusQueue = xQueueCreate(wateringStatusQueueLength, sizeof(WateringStatus));
    // Serial commander task
#if defined(ENABLE_SERIAL_COMMANDS)
  xTaskCreatePinnedToCore(
    serialPortHandler,     // Task function
    "SerialPort",          // Name of the task (for debugging)
    89000,                 // Stack size (in words, not bytes)
    NULL,                  // Task input parameter
    PRIORITY_HIGH,         // Priority of the task
    &serialTaskHandle,     // Task handle
    1                      // Core where the task should run
  );
#endif

#if defined(ONLINE)
  String ssid = config["network"]["ssid"].isNull() ? WIFI_SSID : config["network"]["ssid"].as<String>();
  String password = config["network"]["password"].isNull() ? WIFI_PASSWORD : config["network"]["password"].as<String>();
  TRACE("ssid: %s\n", ssid.c_str());
  TRACE("password: %s\n", password.c_str());
  TRACE("config: %s\n", config.as<String>().c_str());
  if (config["network"]["enabled"].as<bool>() && connectToWiFi(ssid.c_str(), password.c_str())) {
    IPAddress ip = WiFi.localIP();
    TRACE("\n");
    TRACE("Wifi Connected: IP: %s - Hostname: %s\n", WiFi.localIP().toString().c_str(), WiFi.getHostname());

    // Ask for the current time using NTP request builtin into ESP firmware.
    TRACE("Setup ntp...\n");
    initTime(TIMEZONE);
    printLocalTime();

    // Create a task for handling OTA
#if defined(ENABLE_OTA)
    xTaskCreatePinnedToCore(
      handleOTATask,          // Function to implement the task
      "OtaTask",              // Name of the task
      46000,                  // Stack size in words
      NULL,                   // Task input parameter
      PRIORITY_LOW,           // Priority of the task
      &otaTaskHandle,         // Task handle
      1                       // Core where the task should run
    );
#endif
    // Create a task for handling Web Server
#if defined(ENABLE_HTTP)
    xTaskCreatePinnedToCore(
      handleWebServerTask,    // Function to implement the task
      "WebServerTask",        // Name of the task
      46000,                  // Stack size in words
      NULL,                   // Task input parameter
      PRIORITY_MEDIUM,        // Priority of the task
      &webServerTaskHandle,   // Task handle
      1                       // Core where the task should run
    );
#endif

  } else if (config["network"]["enabled"].as<bool>()) {
    TRACE("Wifi not connected!\n");  
    beep(2);
    handleWifiConnectionError("WiFi connection error", settings);
  } else {
    TRACE("Wifi disabled!\n");
  }
#endif

  // Good To Go!
  beep(1);
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

/**
 * i2c Port Extender setup 
 */
bool setupMcp() {
  // uncomment appropriate mcp.begin
  if (!mcp.begin_I2C()) {
    TRACE("MCP I2c Error\n");
    return false;
  }
  for (uint8_t i = 0; i < I2C_MCP_PINCOUNT; i++) {
    mcp.pinMode(i, OUTPUT); // Set all pins as OUTPUT
    mcp.digitalWrite(i, HIGH); // Set all GPIO pins to HIGH
  }
  mcp.writeGPIOAB(0b1111111111111111);
  return true;
}

/**
 * Pulse counter interrupt service
 */
void pulseCounter() {
  // Increment the pulse counter
  int value = digitalRead(FLOW_METER_PIN);
  if (FLOW_SENSOR_STATE != value) {
    FLOW_METER_PULSE_COUNT++;
    FLOW_METER_TOTAL_PULSE_COUNT++;
    FLOW_SENSOR_STATE = value;
  }
}

void calcFlow() {
  // Note the time this processing pass was executed. Note that because we've
  // disabled interrupts the millis() function won't actually be incrementing right
  // at this point, but it will still return the value it was set to just before
  // interrupts went away.
  OLD_INT_TIME = millis();

  while((millis() - OLD_INT_TIME) < 1000) { 
    // Disable the interrupt while calculating flow rate and sending the value to the host
    detachInterrupt(FLOW_METER_INTERRUPT);

    // Because this loop may not complete in exactly 1 second intervals we calculate the 
    // number of milliseconds that have passed since the last execution and use that to 
    // scale the output. We also apply the calibrationFactor to scale the output based on 
    // the number of pulses per second per units of measure (litres/minute in this case) 
    // coming from the sensor.
    FLOW_RATE = ((1000.0 / (millis() - OLD_INT_TIME)) * FLOW_METER_PULSE_COUNT) / FLOW_CALIBRATION_FACTOR;

    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    FLOW_MILLILITRES = FLOW_METER_PULSE_COUNT == 0 ? 0 : (FLOW_RATE / 60) * 1000;

    // Add the millilitres passed in this second to the cumulative total
    TOTAL_MILLILITRES += FLOW_MILLILITRES;

    // Display flow
    displayFlow();

    // Reset the pulse counter so we can start incrementing again
    FLOW_METER_PULSE_COUNT = 0;
    // Enable the interrupt again now that we've finished sending output
    attachInterrupt(FLOW_METER_INTERRUPT, pulseCounter, FALLING);
  }
}

void displayFlow() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.printf("Raw: %.5f\n", FLOW_RATE);
  display.printf("Flow: %.2fmL/s\n", FLOW_MILLILITRES);
  display.printf("Total: %lumL\n", TOTAL_MILLILITRES);
  display.printf("Secs: %d\n", (millis() - START_INT_TIME) / 1000);
  
  display.setCursor(0, 0);
  display.display(); // actually display all of the above
}

void setTimezone(String timezone) {
  TRACE("Setting Timezone to: %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(String timezone) {
  tm timeinfo;
  // Initialize NTP time sync
  TRACE("Setting up time\n");
  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  // Now we can set the real timezone
  setTimezone(timezone);
  if(!getLocalTime(&timeinfo)){
    TRACE("Failed to obtain time\n");
    return;
  }
  TRACE("NTP TIME: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  if (settings.hasRTC) {
    syncRTC();
  } else {
    // Convert to Unix time
    time_t now = mktime(&timeinfo);
    TRACE("Unix Time: %ld\n", now);
  }
}

/**
 * Get the offset between the RTC and the Unix time.
 *
 * @return the offset in seconds
 */
long int getRtcOffset() {
  tm timeinfo;
  getLocalTime(&timeinfo);
  time_t now = mktime(&timeinfo);
  return rtc.now().unixtime() - now;
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    TRACE("Failed to obtain time 1\n");
    return;
  }
  PRINTLN(&timeinfo, "Local Time: %A, %B %d %Y %H:%M:%S zone %Z %z ");
}

void printRtcTime() {
  DateTime now = rtc.now();
  TRACE("%04d/%02d/%02d %02d:%02d:%02d\n", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void displayTime() {
  // text display tests
  DateTime now = rtc.now();
  char time[64];
  sprintf(time, "TIME: %02d:%02d:%02d A:%d", now.hour(), now.minute(), now.second(), getActiveAlarmId(settings, rtc.now()));
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(time);
  display.print("IP: "); display.println(WiFi.localIP());
  display.print("Temp: "); display.print(rtc.getTemperature());  display.println(" C");
  display.printf("Uptime: %s\n", uptimeStr().c_str()); display.println();
  
  display.setCursor(0, 0);
  display.display(); // actually display all of the above
}

bool connectToWiFi(const char* ssid, const char* password, int max_tries, int pause) {
  int i = 0;
  // allow to address the device by the given name e.g. http://webserver
  // Set WiFi mode to Station (Client)
  WiFi.mode(WIFI_STA);
  // Disconnect any existing WiFi connections
  WiFi.disconnect();
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  // Set the hostname
  WiFi.setHostname(settings.hostname);

  
  #if defined(ARDUINO_ARCH_ESP8266)
    WiFi.forceSleepWake();
    delay(200);
  #endif
  // Begin the WiFi connection
  WiFi.begin(ssid, password);
  do {
    vTaskDelay(pause / portTICK_PERIOD_MS);
    TRACE(".");
    i++;
  } while (!WiFi.status() == WL_CONNECTED && i < max_tries);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Initialize mDNS
  TRACE("Setting up MDNS responder!\n");
  i = 0;
  do {
    vTaskDelay(pause / portTICK_PERIOD_MS);
    TRACE(".");
    i++;
  } while (!MDNS.begin(settings.hostname) && i < max_tries);

  return WiFi.status() == WL_CONNECTED;
}

void handleWifiConnectionError(String error, Settings settings, bool restart) {
  TRACE("Error: %s\n", error.c_str());
  if (settings.rebootOnWifiFail) {
    TRACE("Rebooting now...\n");
    vTaskDelay(150 / portTICK_PERIOD_MS);
    ESP.restart();
  }
}

void syncRTC() {
  // Get current time from NTP server
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  // Set RTC time
  DateTime dateTime = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  rtc.adjust(dateTime);
  settings.lastDateTimeSync = rtc.now().unixtime();
  settings.updatedOn = rtc.now().unixtime();
  EEPROM.put(EEPROM_SETTINGS_ADDRESS, settings);
  EEPROM.commit();
  TRACE("RTC synced with NTP time\n");
}

// This function is called when the sysInfo service was requested.
void handleSystemInfo() {
  String result;

  result += "{\n";
  result += "  \"chipModel\": \"" + String(ESP.getChipModel()) + "\",\n";
  result += "  \"chipCores\": " + String(ESP.getChipCores()) + ",\n";
  result += "  \"chipRevision\": " + String(ESP.getChipRevision()) + ",\n";
  result += "  \"flashSize\": " + String(ESP.getFlashChipSize()) + ",\n";
  result += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
  result += "  \"heapSize\": " + String(esp_get_free_heap_size()) + ",\n";
  result += "  \"SSID\": \"" + String(WIFI_SSID) + "\",\n";
  // result += "  \"hotspots\": " + scanWifiNetworks() + ",\n";
  result += "  \"signalDbm\": " + String(WiFi.RSSI()) + ",\n";
  if (settings.hasRTC) {
    result += "  \"temperature\": " + String(rtc.getTemperature()) + ",\n";
    result += "  \"timestamp\": " + String(rtc.now().unixtime()) + ",\n";
    result += "  \"offset\": " + String(getRtcOffset()) + ",\n";
    
  } else {
    tm timeInfo;
    if(!getLocalTime(&timeInfo)){
      TRACE("Failed to obtain time\n");
    } else {
      time_t now = mktime(&timeInfo);
      result += "  \"timestamp\": " + String(now) + ",\n";
    }
  }
  result += "  \"uptime\": \"" + uptimeStr() + "\",\n";
  result += "  \"resetReason\": \"" + String(getResetReason()) + "\",\n";
  result += "  \"timezone\": \"" + String(TIMEZONE) + "\",\n";
  // result += "  \"i2cBusDevices:\": " + String(getI2cDeviceList()) + ",\n";
  if (settings.hasMCP) {
    result += "  \"mcp\": " + String(mcp.readGPIOAB()) + ",\n";
  }
  result += "  \"watering\": {\n";
  result += "    \"totalMillilitres\": " + String(TOTAL_MILLILITRES) + ",\n";
  result += "    \"totalFlowPulses\": " + String(FLOW_METER_TOTAL_PULSE_COUNT) + "\n";
  result += "  },\n";
  result += "  \"sdcard\": {\n";
  result += "    \"cardType\": " + String(SD.cardType()) + ",\n";
  result += "    \"cardSize\": " + String(SD.cardSize() / (1024 * 1024)) + ",\n";
  result += "    \"freeSize\": " + String(SD.cardSize() / (1024 * 1024) - (SD.usedBytes() / (1024 * 1024))) + ",\n";
  result += "    \"logCount\": " + String(getLogCount("/logs")) + "\n";
  result += "  },\n";
  JsonDocument config = readConfig();
  if(!config.isNull()) {
    result += "  \"config\": {\n";
    result += "    \"network\": {\n";
    result += "      \"enabled\": \"" + String(config["network"]["enabled"].as<bool>() ? "true" : "false") + "\",\n";
    result += "      \"ssid\": \"" + config["network"]["ssid"].as<String>() + "\",\n";
    result += "      \"password\": \"" + config["network"]["password"].as<String>() + "\"\n";
    result += "    }\n";
    result += "  },\n";
  }
  result += "  \"settings\":" + settingsToJson(settings) + ",\n";
  result += "  \"env\": {\n";
  uint32_t minTimeToNextAlarm = getNextAlarmTime(settings, rtc.now());
  result += "    \"nextAlarmSecs\":" + String(minTimeToNextAlarm) + ",\n";
  result += "    \"nextAlarm\": \"" + addTimeInterval(minTimeToNextAlarm, rtc.now()) + "\",\n";
  result += "  }\n";
  result += "}";

  TOTAL_MILLILITRES = 0;
  FLOW_METER_TOTAL_PULSE_COUNT = 0;

  server.sendHeader("Cache-Control", "no-cache");
  SERVER_RESPONSE_OK(result);
  return;
}

void handleRoot() {
  // Send the HTML page
  String html = "<!DOCTYPE html><html><body>";
  html += "<h1>ESP Web Server Example</h1>";
  html += "<button onclick=\"fetchData()\">Fetch Data</button>";
  html += "<pre id=\"data\"></pre>";
  html += "<script>function fetchData() { fetch('/api/systeminfo').then(response => response.json()).then(data => document.getElementById('data').innerText = JSON.stringify(data)); }</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleAlarm() {
  if (server.method() == HTTP_GET) {
    String result;

    result += "{\n";
    result += "  \"alarm\": " + getAlarms(settings) + "\n";
    result += "}";

    server.sendHeader("Cache-Control", "no-cache");
    SERVER_RESPONSE_OK(result);
  } else if (server.method() == HTTP_POST) {
    JsonDocument json;
    DeserializationError error = deserializeJson(json, server.arg("plain"));
    
    if (error) {
      TRACE("deserializeJson() failed:\n");
      TRACE(error.c_str());
      SERVER_RESPONSE_ERROR(500, "Serialization error");
      return;
    }

    if(!saveAlarms(json, settings.alarm)) {
      SERVER_RESPONSE_ERROR(500, "Serialization error");
      return;
    };
      
    settings.updatedOn = rtc.now().unixtime();
    EEPROM.put(EEPROM_SETTINGS_ADDRESS, settings);
    EEPROM.commit();

    String result;

    result += "{\n";
    result += "  \"alarm\": " + getAlarms(settings) + "\n";
    result += "}";

    server.sendHeader("Cache-Control", "no-cache");
    SERVER_RESPONSE_OK(result);
  } else {
    SERVER_RESPONSE_ERROR(405, "Method Not Allowed");
  }
  return;
}

void handleLogs() {
  if (server.method() == HTTP_GET) {
    String result;
    String contents = "";

    if(!initSDCard()) {
      SERVER_RESPONSE_ERROR(500, "SD not working");
    };
    
    File file = SD.open("/HelloWorld.txt");
      
    if (!file) {
      SERVER_RESPONSE_ERROR(404, "Failed to open file");
      return;
    }
    
    while (file.available()) {
      contents += file.readString();
    }
    
    file.close();

    DateTime now = rtc.now();

    result += "{\n";
    result += "  \"files\": " + listDirectory2("/logs") + ",\n";
    result += "  \"count\": " + String(getLogCount("/logs")) + ",\n";
    result += "  \"content\":  \"" + contents + "\"\n";
    result += "}";

    saveLog(now, contents, 1234, 1000, 3);
    server.sendHeader("Cache-Control", "no-cache");
    SERVER_RESPONSE_OK(result);
    return;
  } else if (server.method() == HTTP_POST) {

    String result;
    // String alarm = printAlarm(settings);

    result += "{\n";
    result += "  \"files\": " + listDirectory("/logs") + "\n";
    result += "}";

    server.sendHeader("Cache-Control", "no-cache");
    SERVER_RESPONSE_OK(result);
    return;
  } else if (server.method() == HTTP_DELETE) {
    String result;
    // String alarm = printAlarm(settings);

    result += "{\n";
    result += "  \"files\": " + listDirectory("/logs") + "\n";
    result += "}";
  
    server.sendHeader("Cache-Control", "no-cache");
    SERVER_RESPONSE_OK(result);
  } else {
    SERVER_RESPONSE_ERROR(405, "Method Not Allowed");
    return;
  }
  return;
}

void handleTestAlarm() {
  waterPlants();
  SERVER_RESPONSE_OK("{\"success\":true}");
  return;
}

void handleNotFound() {
  SERVER_RESPONSE_ERROR(404, "Not Found");
}

void handleSaveSettings() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("plain") == false) {
      // handle error here
      SERVER_RESPONSE_ERROR(400, "Invalid value");
    }
    JsonDocument json;
    DeserializationError error = deserializeJson(json, server.arg("plain"));
    
    if (error) {
      SERVER_RESPONSE_ERROR(400, "Invalid JSON");
      return;
    }

    String hostname = json["hostname"];
    if (!hostname.isEmpty()) {
      TRACE("Setting hostname %s", hostname.c_str());
      memcpy(settings.hostname, hostname.c_str(), HOSTNAME_MAX_LENGTH);
      WiFi.setHostname(hostname.c_str());
    }
    settings.id = json["id"];
    settings.updatedOn = rtc.now().unixtime();
    EEPROM.put(EEPROM_SETTINGS_ADDRESS, settings);
    EEPROM.commit();
    SERVER_RESPONSE_OK("{\"success\":true}");
  } else {
    SERVER_RESPONSE_ERROR(405, "Method Not Allowed");
  }
}

void handlePlants() {
  if (server.method() == HTTP_GET) {
    String result;

    result += "{\n";
    result += "  \"plants\": " + getPlants(settings) + "\n";
    result += "}";

    server.sendHeader("Cache-Control", "no-cache");
    SERVER_RESPONSE_OK(result);
  } else if (server.method() == HTTP_POST) {

    JsonDocument json;
    DeserializationError error = deserializeJson(json, server.arg("plain"));

    if (error) {
      TRACE("deserializeJson() failed:\n");
      TRACE(error.c_str());
      SERVER_RESPONSE_ERROR(500, "Serialization error");
      return;
    }

    if(!savePlants(json, settings.plant)) {
      SERVER_RESPONSE_ERROR(500, "Serialization error");
      return;
    };
      
    settings.updatedOn = rtc.now().unixtime();
    EEPROM.put(EEPROM_SETTINGS_ADDRESS, settings);
    EEPROM.commit();

    String result;

    result += "{\n";
    result += "  \"plants\": " + getPlants(settings) + "\n";
    result += "}";

    server.sendHeader("Cache-Control", "no-cache");
    SERVER_RESPONSE_OK(result);
  } else {
    SERVER_RESPONSE_ERROR(405, "Method Not Allowed");
  }
  return;
}

void loop() {
  TaskHandle_t alarmTask;
  if (settings.hasRTC) {
    int activateAlarm = getActiveAlarmId(settings, rtc.now());

    if (activateAlarm > -1 && !IS_ALARM_ON) {
      IS_ALARM_ON = true;
      xTaskCreate(
        pumpWater,            // Task function
        "AlarmTask",          // Task name
        46000,                // Stack size (bytes)
        NULL,                 // Task parameter
        tskIDLE_PRIORITY + 4, // Task priority (very high)
        &alarmTask            // Task handle
      );
    } else if (settings.hasDisplay && activateAlarm <= -1 || !IS_ALARM_ON) {
      if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) { 
        displayTime();
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
      xSemaphoreGive(i2cMutex);
    }
  }
  vTaskDelay(500 / portTICK_PERIOD_MS);
}

// Task for handling OTA
void handleOTATask(void * parameter) {
  // // Set up the OTA end callback
  ArduinoOTA.onEnd([]() {
    beep(4);
    TRACE("OTA update successful, rebooting...\n");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP.restart();
  });

  ArduinoOTA.setHostname(settings.hostname);
  // Initialize OTA
  ArduinoOTA.begin();
  for(;;) {
    ArduinoOTA.handle();
    vTaskDelay(10 / portTICK_PERIOD_MS ); // Give some time for the other tasks
  }
}

// Task for handling Web Server
void handleWebServerTask(void * parameter) {
  // Enable CORS header in webserver results
  server.enableCORS(true);
  // REST Endpoint (Only if Connected)
  server.onNotFound(handleNotFound);
  server.on("/", handleRoot);
  server.on("/api/plants", handlePlants);
  server.on("/api/beep", []{
    beep(2, 150);
    SERVER_RESPONSE_SUCCESS();
  });
  server.on("/api/alarm", handleAlarm);
  server.on("/api/systeminfo", HTTP_GET, handleSystemInfo);
  server.on("/api/settings", HTTP_POST, handleSaveSettings);
  server.on("/api/test-alarm", HTTP_GET, handleTestAlarm);
  server.on("/api/logs", HTTP_GET, handleLogs);

  // Start Server
  server.begin();

  TRACE("open <http://%s> or <http://%s>\n", WiFi.getHostname(), WiFi.localIP().toString().c_str());
  
  for(;;) {
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
      server.handleClient();
    }
    xSemaphoreGive(i2cMutex);
    vTaskDelay(150 / portTICK_PERIOD_MS); // Give some time for the other tasks
  }
}

void pumpWater(void *parameter) {
  waterPlants();
  while(getActiveAlarmId(settings, rtc.now()) > -1) {
    displayTime();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  IS_ALARM_ON = false;
  vTaskDelete(NULL);
}

void setWateringStatus(WateringStatus *status) {
  // WateringStatus wateringStatus;
  // wateringStatus.status = status->status;
  // Try to add item to queue for 10 ticks, fail if queue is full
  if (xQueueSend(wateringStatusQueue, status, portMAX_DELAY) != pdPASS) {
    TRACE("wateringStatus queue full\n");
  }
}

void stopWatering() {
  // TOTAL_MILLILITRES = 0;
  FLOW_METER_PULSE_COUNT = 0;
  detachInterrupt(FLOW_METER_INTERRUPT);
  // Turn Pump Off
  mcp.writeGPIOAB(0b1111111111111111);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  setWateringStatus(0);
}

/*
 * unsigned int duration in seconds
 */
void waterPlant(uint8_t valve, unsigned int duration, unsigned long millilitres) {
  struct WateringStatus wateringStatus;
  memset(&wateringStatus, 0, sizeof(WateringStatus));
  wateringStatus.plant = valve;
  // Open Valve
  mcp.pinMode(valve, OUTPUT);
  mcp.digitalWrite(valve, LOW);
  // Wait time to avoid current surge
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  wateringStatus.status = 2;
  setWateringStatus(&wateringStatus);
  // Start the pump
  mcp.pinMode(PUMP1_PIN, OUTPUT);
  mcp.digitalWrite(PUMP1_PIN, LOW);

  pinMode(FLOW_METER_PIN, INPUT);
  pinMode(FLOW_METER_PIN, INPUT_PULLUP);
  wateringStatus.status = 3;
  setWateringStatus(&wateringStatus);
  /*The Hall-effect sensor is connected to pin 
  2 which uses interrupt 0. Configured to trigger on a FALLING state change (transition from HIGH
  (state to LOW state)*/
  TOTAL_MILLILITRES = 0;
  START_INT_TIME = millis();
  attachInterrupt(FLOW_METER_INTERRUPT, pulseCounter, FALLING);
  FLOW_METER_PULSE_COUNT = 0;
  for(uint8_t i = 0; i < duration; i++) {
    calcFlow();
    wateringStatus.flow = TOTAL_MILLILITRES;
    wateringStatus.status = 4;
    setWateringStatus(&wateringStatus);
    if (TOTAL_MILLILITRES > millilitres) {
      break;
    }
  }
  END_INT_TIME = millis();
#if defined(ENABLE_LOGGING)
  saveLog(rtc.now(), "water", valve, TOTAL_MILLILITRES, duration);
#endif
  // TOTAL_MILLILITRES = 0;
  FLOW_METER_PULSE_COUNT = 0;
  detachInterrupt(FLOW_METER_INTERRUPT);
  // Turn Pump Off
  mcp.digitalWrite(PUMP1_PIN, HIGH);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  // Turn Valve Off
  mcp.digitalWrite(valve, HIGH);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  wateringStatus.status = 5;
  wateringStatus.duration = END_INT_TIME - START_INT_TIME;
  setWateringStatus(&wateringStatus);
}

void waterPlants() {
  struct WateringStatus wateringStatus;
  memset(&wateringStatus, 0, sizeof(WateringStatus));
  
  wateringStatus.status = 1;
  setWateringStatus(&wateringStatus);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector   
  for(int plantIndex = 0; plantIndex < SETTINGS_MAX_PLANTS; plantIndex++) {
    Plant plant = settings.plant[plantIndex];
    if (plant.status == 1) {
      waterPlant(plant.id, 20, 250);
    }
  }
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); //enable brownout
  wateringStatus.status = 0;
  setWateringStatus(&wateringStatus);
}

void serialLog(String message) {
  DateTime now = rtc.now();
  message.replace('\n', ' ');
  TRACE("[%04d/%02d/%02d %02d:%02d:%02d] > %s\n", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), message.c_str());
}

// Function to handle serial communication in a FreeRTOS task
void serialPortHandler(void *pvParameters) {
  uint8_t timer = 0;
  TaskHandle_t alarmTask = NULL;
  uint8_t status = 0;
  struct WateringStatus wateringStatus;
  memset(&wateringStatus, 0, sizeof(WateringStatus));
  while (true) {
    if (xQueueReceive(wateringStatusQueue, (void *)&wateringStatus, 0) == pdTRUE) {
      if (status != wateringStatus.status) {
        status = wateringStatus.status;
      }
    }

    if (Serial.available() > 0) {
      String command = Serial.readStringUntil('\n');
      Serial.flush();
      command.trim();
      if (command.equals("ping")) {
        serialLog(String("pong!"));
      } else if (command.equals("beep")) {
        serialLog(String("beep!"));
        beep(8, 40);
      } else if (command.equals("water")) {
        serialLog(String("Start watering plants!"));
        if(xTaskCreate(
          pumpWater,            // Task function
          "AlarmTask",          // Task name
          46000,                // Stack size (bytes)
          NULL,                 // Task parameter
          PRIORITY_HIGH,        // Task priority (very high)
          &alarmTask            // Task handle
        ) != pdPASS) {
          serialLog(String("Task creation failed!"));
        }
        serialLog(String("Started watering plants!"));
      } else if (command.equals("watering-status")) {
        serialLog(String("plant: " + String(wateringStatus.plant) + " status: " + String(status) + " flow: " + String(wateringStatus.flow)));
      } else if (command.equals("time")) {
        DateTime now = rtc.now();
        Serial.printf("%04d/%02d/%02d %02d:%02d:%02d\n", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
      } else if (command.equals("alarm")) {
        serialLog(getAlarms(settings));
      } else if (command.length() > 0) {
        serialLog(String("Invalid command"));
      } else {
        serialLog(String("Unknown input"));
      }
    }
    if (timer >= 100) {
      // Runs once a second
      timer = 0;
    } else {
      timer += 1;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Small delay to yield task
  }
}
