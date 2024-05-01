#include "main.h"

void setupMcp() {
  // uncomment appropriate mcp.begin
  if (!mcp.begin_I2C()) {
    TRACE("MCP I2c Error\n");
  }
  for (uint8_t i = 0; i < I2C_MCP_PINCOUNT; i++) {
    mcp.pinMode(i, OUTPUT); // Set all pins as OUTPUT
    mcp.digitalWrite(i, HIGH); // Set all GPIO pins to HIGH
  }
  mcp.writeGPIOAB(0b1111111111111111);
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
    TRACE("EEPROM not found or not ready");
    beep(2);
    // ESP.restart();
  }

  if (!rtc.begin()) {
    TRACE("Couldn't find RTC");
    // Serial.flush();
    beep(2);
    // abort();
  }

  if (rtc.lostPower()) {
    TRACE("RTC lost power, let's set the time!\n");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    beep(2);
  }


  EEPROM.begin(EEPROM_SIZE);

  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {; // Address 0x3C for 128x32
    TRACE("Display not working\n");
    // Clear the buffer.
    display.clearDisplay();
    display.display();
  }

  // Setup i2c port extender
  setupMcp();

  printI2cDevices();

  // EEPROM
  EEPROM.get(EEPROM_SETTINGS_ADDRESS, settings);
  TRACE("EEPROM2: %s now: %d\n", settings.hostname, rtc.now().unixtime());

  connectToWiFi(WIFI_SSID, WIFI_PASSWORD);
  
  if (isConnected()) {
    ip = WiFi.localIP();
    TRACE("\n");
    TRACE("Connected: "); PRINT(ip); TRACE("\n");
  } else {
    TRACE("\n");  
    beep(2);  
    errorMsg("Error connecting to WiFi");
  }

  // Ask for the current time using NTP request builtin into ESP firmware.
  TRACE("Setup ntp...\n");
  initTime(TIMEZONE);   // Set for Melbourne/AU
  printLocalTime();

  // Enable CORS header in webserver results
  server.enableCORS(true);
  // REST Endpoint (Only if Connected)
  server.on("/api/sysinfo", HTTP_GET, handleSysInfo);
  server.on("/api/settings", HTTP_POST, handleSaveSettings);
  // fetch('http://192.168.0.152/api/valve', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ pin: 9, value: 'off' }) });
  server.on("/api/valve", HTTP_POST, handleValve);
  server.on("/api/pump", HTTP_POST, handlePump);
  server.on("/api/test-flow", HTTP_GET, handleTestFlow);
  server.on("/api/alarm", HTTP_GET, handleAlarm);
  server.on("/api/alarm", HTTP_POST, handleAlarm);
  // Start Server
  server.begin();

  TRACE("open <http://%s> or <http://%s>\n", WiFi.getHostname(), WiFi.localIP().toString().c_str());

  beep(1);

  displayTime();
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  // xTaskCreate(
  //   taskFunction,       // Task function
  //   "AlarmTask",        // Task name
  //   2048,               // Stack size (bytes)
  //   NULL,               // Task parameter
  //   1,                  // Task priority
  //   NULL                // Task handle
  // );
  /*The Hall-effect sensor is connected to pin 2 which uses interrupt 0. Configured to trigger on a FALLING state change (transition from HIGH
  (state to LOW state)*/
  // attachInterrupt(FLOW_METER_INTERRUPT, pulseCounter, FALLING); //you can use Rising or Falling
  // //create a task that executes the Task0code() function, with priority 1 and executed on core 0
  // xTaskCreatePinnedToCore(Task0code, "Task0", 10000, NULL, 1, &Task0, 0);
  // //create a task that executes the Task0code() function, with priority 1 and executed on core 1
  // xTaskCreatePinnedToCore(Task1code, "Task1", 10000, NULL, 1, &Task1, 1);
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

void displayFlow() {
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

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    display.printf("Raw: %.5f\n", FLOW_RATE);

    // display.print("Flow rate: ");
    // display.print(FLOW_MILLILITRES, DEC);  // Print the integer part of the variable
    // display.println(" mL/s");

    display.printf("Flow: %.2fmL/s\n", FLOW_MILLILITRES);

    // display.print("Total: ");        
    // display.print(TOTAL_MILLILITRES, DEC);
    // display.println(" mL");

    // display.printf("Total: %lumL\n", TOTAL_MILLILITRES);
    display.printf("Total: %lumL\n", TOTAL_MILLILITRES);

    // display.print("Secs: ");        
    // display.print((millis() - OLD_INT_TIME), DEC);
    // display.println("");

    display.printf("Secs: %d\n", millis() - OLD_INT_TIME);
    
    display.setCursor(0, 0);
    display.display(); // actually display all of the above

    // Reset the pulse counter so we can start incrementing again
    FLOW_METER_PULSE_COUNT = 0;
    // Enable the interrupt again now that we've finished sending output
    attachInterrupt(FLOW_METER_INTERRUPT, pulseCounter, FALLING);
  }
}

void setTimezone(String timezone) {
  TRACE("Setting Timezone to: %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(String timezone) {
  Settings *settings = (Settings *) malloc(sizeof(Settings));
  // DateTime dateTime;
  tm localTime;
  TRACE("Setting up time\n");
  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  if(!getLocalTime(&localTime)){
    TRACE("Failed to obtain time\n");
    return;
  }
  TRACE("Got the time from NTP\n");
  // Now we can set the real timezone
  setTimezone(timezone);
  TRACE("TIME: %02d:%02d:%02d\n", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
  syncRTC();
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    TRACE("Failed to obtain time 1\n");
    return;
  }
  PRINTLN(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
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
  sprintf(time, "TIME: %02d:%02d:%02d", now.hour(), now.minute(), now.second() );
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("SSID: "); display.println(WIFI_SSID);
  display.print("IP: "); display.println(ip);
  // display.print("C: "); display.println(rtc.getTemperature());
  if (DISPLAY_INFO_DATA == 0) {
    display.print("Alarm: "); display.println(String(isAlarmOn(settings, rtc.now())));
  } else if (DISPLAY_INFO_DATA == 1) {
    display.print("Week: "); display.println( String((8 & (1 << rtc.now().dayOfTheWeek()))) && (rtc.now().minute() > settings.alarm[0][0][2])    );
  }
  // display.print("Weekday: "); display.println(String(now.dayOfTheWeek()));
  // display.print("mask: "); display.println(1 << now.dayOfTheWeek());
  // display.print("cond: "); display.println((4 & 1 << now.dayOfTheWeek()) != 0);
  display.println(time);
  display.setCursor(0, 0);
  display.display(); // actually display all of the above
  free(time);
}

void turnOnPin(int pinNumber) {
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

bool isConnected() {
  return (WiFi.status() == WL_CONNECTED);
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

void get2cDevices(byte* devices) {
  byte error, address;
  int nDevices;

  memset(devices, 0, sizeof(byte) * MAX_I2C_DEVICES);
 
  TRACE("Scanning...\n");
 
  nDevices = 0;
  for(address = 1; address < MAX_I2C_DEVICES; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0)
    {
      TRACE("I2C device found at address 0x");
      devices[nDevices] = address;
      if (devices[nDevices] < 16)
        TRACE("0");
      PRINT(devices[nDevices], HEX);
      TRACE("  !\n");
      nDevices++;
    }
    else if (error == 4)
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
    TRACE("Done\n");
 
  return;
}

String getI2cDeviceList() {
  String result = "[";
  byte* i2cDevices = (byte*)malloc(sizeof(byte) * MAX_I2C_DEVICES);
  get2cDevices(i2cDevices);
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

// This function is called when the sysInfo service was requested.
void handleSysInfo() {
  String result;
  String alarm = printAlarm(settings);

  result += "{\n";
  result += "  \"chipModel\": \"" + String(ESP.getChipModel()) + "\",\n";
  result += "  \"chipCores\": " + String(ESP.getChipCores()) + ",\n";
  result += "  \"chipRevision\": " + String(ESP.getChipRevision()) + ",\n";
  result += "  \"flashSize\": " + String(ESP.getFlashChipSize()) + ",\n";
  result += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
  result += "  \"temperature\": " + String(rtc.getTemperature()) + ",\n";
  result += "  \"SSID\": \"" + String(WIFI_SSID) + "\",\n";
  result += "  \"signalDbm\": " + String(WiFi.RSSI()) + ",\n";
  result += "  \"time\": " + String(rtc.now().unixtime()) + ",\n";
  result += "  \"timezone\": \"" + String(TIMEZONE) + "\",\n";
  result += "  \"i2cDevices\": \"" + String(getI2cDeviceList()) + "\",\n";
  result += "  \"mcp\": \"" + String(mcp.readGPIOAB()) + "\",\n";
  result += "  \"watering\": {\n";
  result += "    \"totalMillilitres\": " + String(TOTAL_MILLILITRES) + ",\n";
  result += "    \"totalFlowPulses\": " + String(FLOW_METER_TOTAL_PULSE_COUNT) + "\n";
  result += "  },\n";
  result += "  \"settings\": {\n";
  result += "    \"id\": " + String(settings.id) + ",\n";
  result += "    \"hostname\": \"" + String(settings.hostname) + "\",\n";
  result += "    \"lastDateTimeSync\": " + String(settings.lastDateTimeSync) + ",\n";
  result += "    \"updatedOn\": " + String(settings.updatedOn) + ",\n";
  result += "    \"alarm\": " + alarm + ",\n";
  result += "    \"rebootOnWifiFail\": " + String(JSONBOOL(settings.rebootOnWifiFail)) + "\n";
  result += "  }\n";
  // result += "  \"fsTotalBytes\": " + String(LittleFS.totalBytes()) + ",\n";
  // result += "  \"fsUsedBytes\": " + String(LittleFS.usedBytes()) + ",\n";
  result += "}";


  FLOW_METER_TOTAL_PULSE_COUNT = 0;

  server.sendHeader("Cache-Control", "no-cache");
  SERVER_RESPONSE_OK(result);
}


void handleAlarm() {
  if (server.method() == HTTP_GET) {
    String result;
    String alarm = printAlarm(settings);

    result += "{\n";
    result += "  \"alarm\": " + alarm + "\n";
    result += "}";

    server.sendHeader("Cache-Control", "no-cache");
    SERVER_RESPONSE_OK(result);
  } else if (server.method() == HTTP_POST) {
    JsonDocument json;
    
    if (deserializeJson(json, server.arg("plain"))) {
      SERVER_RESPONSE_ERROR(400, "{\"error\":\"Invalid JSON\"}");
      return;
    }

    // int id = json["id"];
    // String value = json["alarm"];

    // // Initialize alarm[0][0]
    // settings.alarm[0][0][0] = 0b00000100;
    // settings.alarm[0][0][1] = 21;
    // settings.alarm[0][0][2] = 00;
    // settings.alarm[0][0][3] = 01;

    // // Initialize alarm[0][1]
    // settings.alarm[0][1][0] = 0b00000100;
    // settings.alarm[0][1][1] = 23;
    // settings.alarm[0][1][2] = 59;
    // settings.alarm[0][1][3] = 01;

    setupAlarms(server, settings.alarm);
      
    settings.updatedOn = rtc.now().unixtime();
    EEPROM.put(EEPROM_SETTINGS_ADDRESS, settings);
    EEPROM.commit();

    String result;
    String alarm = printAlarm(settings);

    result += "{\n";
    result += "  \"alarm\": " + alarm + "\n";
    result += "}";

    server.sendHeader("Cache-Control", "no-cache");
    SERVER_RESPONSE_OK(result);
    SERVER_RESPONSE_OK(result);
  } else {
    SERVER_RESPONSE_ERROR(405, "{\"error\":\"Method Not Allowed\"}");
  }
  return;
}

void handleValve() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("plain") == false) {
      // handle error here
      SERVER_RESPONSE_ERROR(400, "{\"error\":\"Invalid value\"}");
      return;
    }
    
    JsonDocument json;
    
    if (deserializeJson(json, server.arg("plain"))) {
      SERVER_RESPONSE_ERROR(400, "{\"error\":\"Invalid JSON\"}");
      return;
    }

    int pin = json["pin"];
    String value = json["value"];

    if (value == "on" && pin >= 0 && pin <= I2C_MCP_PINCOUNT) {
      turnOnPin(pin);
      SERVER_RESPONSE_SUCCESS();
      return;
    } else if (value == "off" && pin >= 0 && pin <= I2C_MCP_PINCOUNT) {
      mcp.digitalWrite(pin, HIGH);
      SERVER_RESPONSE_SUCCESS();
      return;
    } else {
      SERVER_RESPONSE_ERROR(400, "{\"error\":\"Invalid value\"}");
      return;
    }
  } else {
    SERVER_RESPONSE_ERROR(405, "{\"error\":\"Method Not Allowed\"}");
    return;
  }
  return;
}


void handlePump() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("plain") == false) {
      // handle error here
      SERVER_RESPONSE_ERROR(400, "{\"error\":\"Invalid value\"}");
      return;
    }
    
    JsonDocument json;
    
    if (deserializeJson(json, server.arg("plain"))) {
      SERVER_RESPONSE_ERROR(400, "{\"error\":\"Invalid JSON\"}");
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
    //   SERVER_RESPONSE_ERROR(400, "{\"error\":\"Invalid value\"}");
    //   return;
    // }
  } else {
    SERVER_RESPONSE_ERROR(405, "{\"error\":\"Method Not Allowed\"}");
    return;
  }
  return;
}
void handleSaveSettings() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("plain") == false) {
      // handle error here
      SERVER_RESPONSE_ERROR(400, "{\"error\":\"Invalid value\"}");
    }
    JsonDocument json;
    DeserializationError error = deserializeJson(json, server.arg("plain"));
    
    if (error) {
      SERVER_RESPONSE_ERROR(400, "{\"error\":\"Invalid JSON\"}");
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
    SERVER_RESPONSE_ERROR(405, "{\"error\":\"Method Not Allowed\"}");
  }
}

void errorMsg(String error, bool restart) {
  TRACE("Error: %s", error.c_str());
  if (restart) {
    TRACE("Rebooting now...\n");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    if (settings.rebootOnWifiFail) {
      ESP.restart();
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void loop() {
  // static unsigned long oldTime = millis();
  // Only process counters once per second 
  // put your main code here, to run repeatedly:
  // printI2cDevices();
  // displayTime();
  // for (int i = 0; i < 16; i++) {
  //   turnOnPin(i);
  //   vTaskDelay(2000 / portTICK_PERIOD_MS);
  //   displayTime();
  // }
  // vTaskDelay(500 / portTICK_PERIOD_MS);
  server.handleClient();
  // time_t now = time(nullptr);
  // struct tm *timeinfo;
  // timeinfo = localtime(&now);

  // TRACE("Current time: %02d:%02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  // vTaskDelay(1000 / portTICK_PERIOD_MS);
  // if((millis() - oldTime) > 5000) { 
  //   displayTime();
  //   oldTime = millis();
  //   vTaskDelay(1000 / portTICK_PERIOD_MS);
  // }
  // displayFlow();
  // printLocalTime();
  // printRtcTime();
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  bool activateAlarm = isAlarmOn(settings, rtc.now());

  if (activateAlarm && !IS_ALARM_ON) {
    IS_ALARM_ON = true;
    xTaskCreate(
      pumpWater,          // Task function
      "AlarmTask",        // Task name
      8192,               // Stack size (bytes)
      NULL,               // Task parameter
      1,                  // Task priority
      NULL                // Task handle
    );
  } else if (!activateAlarm || !IS_ALARM_ON) {
    displayTime();
  }
}

void pumpWater(void *parameter) {
  handleTestFlow();
  while(isAlarmOn(settings, rtc.now())) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  IS_ALARM_ON = false;
  vTaskDelete(NULL);
}

void handleTestFlow() {
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
    displayFlow();
  }
  detachInterrupt(FLOW_METER_INTERRUPT);
  
  mcp.writeGPIOAB(0b1111111111111111);
  SERVER_RESPONSE_OK("{\"success\":true}");
}
