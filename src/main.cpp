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

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  while (!Serial) {
    vTaskDelay(100 / portTICK_PERIOD_MS); // wait for serial port to connect. Needed for native USB
  }

  Wire.begin();

  // Check if EEPROM is ready
  Wire.beginTransmission(EEPROM_ADDRESS);
  if (Wire.endTransmission() != 0) {
    settings.hasEEPROM = false;
    beep(2);
    TRACE("EEPROM not found or not ready\n");
  } else {
    // Init EEPROM chip in the RTC module
    if (EEPROM.begin(EEPROM_SIZE)) {
      // Read EEPROM settings
      EEPROM.get(EEPROM_SETTINGS_ADDRESS, settings);
      // TRACE("Settings: %s now: %d\n", settings.hostname, rtc.now().unixtime());
    } else {
      settings.hasEEPROM = false;
      TRACE("EEPROM not Working\n");
    };
  }

  if (!rtc.begin()) {
    TRACE("Couldn't find RTC\n");
    beep(2);
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
  };

  if(!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESSS)) {; // Address 0x3C for 128x32
    TRACE("Display not working\n");
    settings.hasDisplay = false;
  } else {
    // Clear display
    display.clearDisplay();
    display.display();
    displayTime();
  }

  if (connectToWiFi(WIFI_SSID, WIFI_PASSWORD)) {
    ip = WiFi.localIP();
    TRACE("\n");
    TRACE("Connected: "); PRINT(ip); TRACE("\n");

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

    // Ask for the current time using NTP request builtin into ESP firmware.
    TRACE("Setup ntp...\n");
    initTime(TIMEZONE);   // Set for Melbourne/AU
    printLocalTime();

    // Enable CORS header in webserver results
    server.enableCORS(true);
    // REST Endpoint (Only if Connected)
    server.on("/", handleRoot);
    server.on("/api/systeminfo", HTTP_GET, handleSystemInfo);
    server.on("/api/settings", HTTP_POST, handleSaveSettings);
    // fetch('http://192.168.0.152/api/valve', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ pin: 9, value: 'off' }) });
    server.on("/api/valve", HTTP_POST, handleValve);
    server.on("/api/pump", HTTP_POST, handlePump);
    server.on("/api/test-flow", HTTP_GET, handleTestFlow);
    server.on("/api/alarm", HTTP_GET, handleAlarm);
    server.on("/api/alarm", HTTP_POST, handleAlarm);
    server.on("/api/logs", HTTP_GET, handleLogs);
    // Start Server
    server.begin();

    TRACE("open <http://%s> or <http://%s>\n", WiFi.getHostname(), WiFi.localIP().toString().c_str());
  } else {
    TRACE("Wifi not connected!\n");  
    beep(2);  
    handleWifiConnectionError("WiFi connection error", settings);
  }

  // Good To Go!
  beep(1);

  // xTaskCreate(
  //   taskFunction,       // Task function
  //   "AlarmTask",        // Task name
  //   2048,               // Stack size (bytes)
  //   NULL,               // Task parameter
  //   1,                  // Task priority
  //   NULL                // Task handle
  // );
}

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
  // Settings *settings = (Settings *) malloc(sizeof(Settings));
  // DateTime dateTime;
  tm timeinfo;
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
  char * time = (char *) malloc(sizeof(char) * 64);
  memset(time, 0, sizeof(char) * 64);
  sprintf(time, "TIME: %02d:%02d:%02d A:%d", now.hour(), now.minute(), now.second(), getActiveAlarmId(settings, rtc.now()));
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("SSID: "); display.println(WIFI_SSID);
  display.print("IP: "); display.println(ip);
  // display.print("C: "); display.println(rtc.getTemperature());
  if (DISPLAY_INFO_DATA == 0) {
    display.print("Temp2: ");  display.print(rtc.getTemperature());  display.println(" C");
  } else if (DISPLAY_INFO_DATA == 1) {
    // display.print("Week: "); display.println(String((8 & (1 << rtc.now().dayOfTheWeek()))));
    display.print("Week: "); display.println(String(1 << rtc.now().dayOfTheWeek()));
  }
  // display.print("Weekday: "); display.println(String(now.dayOfTheWeek()));
  // display.print("mask: "); display.println(1 << now.dayOfTheWeek());
  // display.print("cond: "); display.println((4 & 1 << now.dayOfTheWeek()) != 0);
  display.println(time);
  display.setCursor(0, 0);
  display.display(); // actually display all of the above
  free(time);
}

bool connectToWiFi(const char* ssid, const char* password, int max_tries, int pause) {
  int i = 0;
  // allow to address the device by the given name e.g. http://webserver
  WiFi.setHostname(settings.hostname);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  
  #if defined(ARDUINO_ARCH_ESP8266)
    WiFi.forceSleepWake();
    delay(200);
  #endif
  WiFi.begin(ssid, password);
  do {
    vTaskDelay(pause / portTICK_PERIOD_MS);
    TRACE(".");
    i++;
  } while (!isConnected() && i < max_tries);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Initialize mDNS
  while (!MDNS.begin(settings.hostname)) {
    TRACE("Error setting up MDNS responder!\n");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }

  return isConnected();
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
  result += "  \"SSID\": \"" + String(WIFI_SSID) + "\",\n";
  result += "  \"signalDbm\": " + String(WiFi.RSSI()) + ",\n";
  if (settings.hasRTC) {
    result += "  \"temperature\": " + String(rtc.getTemperature()) + ",\n";
    result += "  \"timestamp\": " + String(rtc.now().unixtime()) + ",\n";
  } else {
    tm timeInfo;
    if(!getLocalTime(&timeInfo)){
      TRACE("Failed to obtain time\n");
    } else {
      time_t now = mktime(&timeInfo);
      result += "  \"timestamp\": " + String(now) + ",\n";
    }
  }
  result += "  \"uptime\": " + String(millis()) + ",\n";
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
  result += "  \"settings\": {\n";
  result += "    \"id\": " + String(settings.id) + ",\n";
  result += "    \"hostname\": \"" + String(settings.hostname) + "\",\n";
  result += "    \"lastDateTimeSync\": " + String(settings.lastDateTimeSync) + ",\n";
  result += "    \"updatedOn\": " + String(settings.updatedOn) + ",\n";
  result += "    \"rebootOnWifiFail\": " + String(JSONBOOL(settings.rebootOnWifiFail)) + ",\n";
  result += "    \"alarm\": " + getAlarms(settings) + "\n";
  result += "  }\n";
  result += "}";


  TOTAL_MILLILITRES = 0;
  FLOW_METER_TOTAL_PULSE_COUNT = 0;

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  SERVER_RESPONSE_OK(result);
}


void handleRoot() {
  // Set CORS headers
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Max-Age", "10000");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  
  // Send the HTML page
  String html = "<!DOCTYPE html><html><body>";
  html += "<h1>ESP Web Server Example</h1>";
  html += "<button onclick=\"fetchData()\">Fetch Data</button>";
  html += "<pre id=\"data\"></pre>";
  html += "<script>function fetchData() { fetch('http://indoor_chico_01.local/api/sysinfo').then(response => response.json()).then(data => document.getElementById('data').innerText = JSON.stringify(data)); }</script>";
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
    
    if (deserializeJson(json, server.arg("plain"))) {
      SERVER_RESPONSE_ERROR(400, "Invalid JSON");
      return;
    }

    if(!setupAlarms(server, settings.alarm)) {
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

void handleValve() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("plain") == false) {
      // handle error here
      SERVER_RESPONSE_ERROR(400, "Invalid value");
      return;
    }
    
    JsonDocument json;
    
    if (deserializeJson(json, server.arg("plain"))) {
      SERVER_RESPONSE_ERROR(400, "Invalid JSON");
      return;
    }

    int pin = json["pin"];
    String value = json["value"];

    if (value == "on" && pin >= 0 && pin <= I2C_MCP_PINCOUNT) {
      turnOnPin(mcp, pin);
      SERVER_RESPONSE_SUCCESS();
      return;
    } else if (value == "off" && pin >= 0 && pin <= I2C_MCP_PINCOUNT) {
      mcp.digitalWrite(pin, HIGH);
      SERVER_RESPONSE_SUCCESS();
      return;
    } else {
      SERVER_RESPONSE_ERROR(400, "Invalid value");
      return;
    }
  } else {
    SERVER_RESPONSE_ERROR(405, "Method Not Allowed");
    return;
  }
  return;
}

void handleLogs() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Max-Age", "10000");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

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
    result += "  \"count\": " + String(getLogCount("/logs")) + "\n";
    result += "  \"content\":  \"" + contents + "\",\n";
    result += "}";

    saveLog(now, contents, 1234, 1000, 3);
    server.sendHeader("Cache-Control", "no-cache");
    SERVER_RESPONSE_OK(result);
    return;
  } else if (server.method() == HTTP_POST) {
    // JsonDocument json;
    
    // if (deserializeJson(json, server.arg("plain"))) {
    //   SERVER_RESPONSE_ERROR(400, "Invalid JSON");
    //   return;
    // }

    // setupAlarms(server, settings.alarm);
      
    // settings.updatedOn = rtc.now().unixtime();
    // EEPROM.put(EEPROM_SETTINGS_ADDRESS, settings);
    // EEPROM.commit();

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

void handlePump() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("plain") == false) {
      // handle error here
      SERVER_RESPONSE_ERROR(400, "Invalid value");
      return;
    }
    
    JsonDocument json;
    
    if (deserializeJson(json, server.arg("plain"))) {
      SERVER_RESPONSE_ERROR(400, "Invalid JSON");
      return;
    }

    int pump = json["pump"];
    String value = json["value"];

    mcp.digitalWrite(PUMP1_PIN, LOW);
    SERVER_RESPONSE_SUCCESS();
    // if (value == "on" && (pump == 1 || pump == 2)) {
    //   uint8_t onPump = pump == 1 ? PUMP1_PIN : PUMP2_PIN;
    //   uint8_t offPump = pump != 1 ? PUMP2_PIN : PUMP1_PIN;
    //   mcp.digitalWrite(onPump, LOW);
    //   mcp.digitalWrite(offPump, HIGH);
    //   SERVER_RESPONSE_SUCCESS();
    //   return;
    // } else if (value == "off" && (pump == 1 || pump == 2)) {
    //   uint8_t port = pump == 1 ? PUMP1_PIN : PUMP2_PIN;
    //   mcp.digitalWrite(port, HIGH);
    //   SERVER_RESPONSE_SUCCESS();
    //   return;
    // } else {
    //   SERVER_RESPONSE_ERROR(400, "Invalid value");
    //   return;
    // }
  } else {
    SERVER_RESPONSE_ERROR(405, "Method Not Allowed");
    return;
  }
  return;
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

void loop() {
  server.handleClient();
  // time_t now = time(nullptr);
  // struct tm *timeinfo;
  // timeinfo = localtime(&now);
  // TRACE("Current time: %02d:%02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  // vTaskDelay(1000 / portTICK_PERIOD_MS);

  // Handle OTA updates
  ArduinoOTA.handle();

  if (settings.hasRTC) {
    int activateAlarm = getActiveAlarmId(settings, rtc.now());

    if (activateAlarm > -1 && !IS_ALARM_ON) {
      IS_ALARM_ON = true;
      xTaskCreate(
        pumpWater,          // Task function
        "AlarmTask",        // Task name
        8192,               // Stack size (bytes)
        NULL,               // Task parameter
        1,                  // Task priority
        NULL                // Task handle
      );
    } else if (activateAlarm <= -1 || !IS_ALARM_ON && settings.hasDisplay) {
      displayTime();
    }
  }
}

void pumpWater(void *parameter) {
  handleTestFlow();
  while(getActiveAlarmId(settings, rtc.now()) > -1) {
    displayTime();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  IS_ALARM_ON = false;
  vTaskDelete(NULL);
}

/*
 * unsigned int duration in seconds
 */
void waterPlant(uint8_t valve, unsigned int duration, unsigned long millilitres) {
  // Turn off all outputs
  mcp.writeGPIOAB(0b1111111111111111);
  vTaskDelay(10 / portTICK_PERIOD_MS);
  mcp.digitalWrite(valve, LOW);
  // Wait time to settle current and start the pump
  vTaskDelay(250 / portTICK_PERIOD_MS);
  mcp.digitalWrite(PUMP1_PIN, LOW);

  pinMode(FLOW_METER_PIN, INPUT);
  pinMode(FLOW_METER_PIN, INPUT_PULLUP);
  /*The Hall-effect sensor is connected to pin 2 which uses interrupt 0. Configured to trigger on a FALLING state change (transition from HIGH
  (state to LOW state)*/
  TOTAL_MILLILITRES = 0;
  START_INT_TIME = millis();
  attachInterrupt(FLOW_METER_INTERRUPT, pulseCounter, FALLING);
  FLOW_METER_PULSE_COUNT = 0;
  for(uint8_t i = 0; i < duration; i++) {
    calcFlow();
    if (TOTAL_MILLILITRES > millilitres) {
      break;
    }
  }
  // TOTAL_MILLILITRES = 0;
  FLOW_METER_PULSE_COUNT = 0;
  detachInterrupt(FLOW_METER_INTERRUPT);
  // Turn all outputs off
  mcp.writeGPIOAB(0b1111111111111111);
}

void handleTestFlow() {
  waterPlant(0, 20, 250);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  waterPlant(1, 20, 250);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  // waterPlant(2, 20, 250);
  // vTaskDelay(500 / portTICK_PERIOD_MS);
  // waterPlant(3, 20, 250);
  SERVER_RESPONSE_OK("{\"success\":true}");
  return;
  mcp.writeGPIOAB(0b1111111111111110);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  mcp.writeGPIOAB(0b1111101111111110);
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  pinMode(FLOW_METER_PIN, INPUT);
  pinMode(FLOW_METER_PIN, INPUT_PULLUP);
  /*The Hall-effect sensor is connected to pin 2 which uses interrupt 0. Configured to trigger on a FALLING state change (transition from HIGH
  (state to LOW state)*/
  attachInterrupt(FLOW_METER_INTERRUPT, pulseCounter, FALLING);
  TOTAL_MILLILITRES = 0;
  for(uint8_t i = 0; i < 20; i++) {
    calcFlow();
  }
  detachInterrupt(FLOW_METER_INTERRUPT);
  
  mcp.writeGPIOAB(0b1111111111111111);
  SERVER_RESPONSE_OK("{\"success\":true}");
}
