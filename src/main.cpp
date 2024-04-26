#include "main.h"


void setupMcp() {
  // uncomment appropriate mcp.begin
  if (!mcp.begin_I2C()) {
    Serial.println("MCP I2c Error.");
  }
  for (uint8_t i = 0; i < I2C_MCP_PINCOUNT; i++) {
    mcp.pinMode(i, OUTPUT); // Set all pins as OUTPUT
    mcp.digitalWrite(i, HIGH); // Set all GPIO pins to LOW
  }
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
    // ESP.restart();
  }

  EEPROM.begin(EEPROM_SIZE);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  // Clear the buffer.
  display.clearDisplay();
  display.display();

  // Setup i2c port extender
  setupMcp();

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
    errorMsg("Error connecting to WiFi");
  }

  // Ask for the current time using NTP request builtin into ESP firmware.
  TRACE("Setup ntp...\n");
  // initTime("WET0WEST,M3.5.0/1,M10.5.0");   // Set for Melbourne/AU
  initTime(TIMEZONE);   // Set for Melbourne/AU
  printLocalTime();

  // REST Endpoint (Only if Connected)
  server.on("/api/sysinfo", HTTP_GET, handleSysInfo);
  server.on("/api/settings", HTTP_POST, handleSaveSettings);
  server.on("/api/valve", HTTP_POST, handleValve);
  // Enable CORS header in webserver results
  server.enableCORS(true);
  // Start Server
  server.begin();

  // TRACE("open <http://%s> or <http://%s>\n", WiFi.getHostname(), WiFi.localIP().toString().c_str());

  printI2cDevices();
}

// TRACE();

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
    Serial.println("Failed to obtain time 1");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
}

void printI2cDevices() {
  byte error, address;
  int nDevices;
 
  Serial.println("Scanning...");
 
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
      Serial.print("I2C device found at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println("  !");
 
      nDevices++;
    }
    else if (error==4)
    {
      Serial.print("Unknown error at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
 
}

void displayTime() {
  // text display tests
  DateTime now = rtc.now();
  char * time = (char *) malloc(sizeof(char) * 64);
  memset(time, 0, sizeof(char) * 64);
  sprintf(time, "Time: %02d:%02d:%02d", now.hour(), now.minute(), now.second() );
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("SSID:"); display.println(WIFI_SSID);
  display.print("IP:"); display.println(ip);
  display.print("C: "), display.println(rtc.getTemperature());
  display.println(time);
  display.setCursor(0,0);
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
  result += "  \"settings\": {\n";
  result += "    \"id\": " + String(settings.id) + ",\n";
  result += "    \"hostname\": \"" + String(settings.hostname) + "\",\n";
  result += "    \"lastDateTimeSync\": " + String(settings.lastDateTimeSync) + ",\n";
  result += "    \"updatedOn\": " + String(settings.updatedOn) + ",\n";
  result += "    \"rebootOnWifiFail\": " + String(settings.rebootOnWifiFail) + "\n";
  result += "  }\n";
  // result += "  \"fsTotalBytes\": " + String(LittleFS.totalBytes()) + ",\n";
  // result += "  \"fsUsedBytes\": " + String(LittleFS.usedBytes()) + ",\n";
  result += "}";

  server.sendHeader("Cache-Control", "no-cache");
  SERVER_RESPONSE_OK(result);
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
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  displayTime();
}

// void writeToEEPROM(int address, void* data, size_t length) {
//   Wire.beginTransmission(EEPROM_ADDRESS);
//   Wire.write((uint8_t)(address >> 8)); // MSB of address
//   Wire.write((uint8_t)(address & 0xFF)); // LSB of address
//   byte* byteData = (byte*)data;
//   TRACE("SAVING: %d\n", length);
//   for (size_t i = 0; i < length; i++) {
//     TRACE("%c", byteData[i]);
//     Wire.write(byteData[i]);
//   }
//   Wire.endTransmission();
//   vTaskDelay(100 / portTICK_PERIOD_MS); // Delay to allow EEPROM write cycle to complete
// }

void readFromEEPROM(int address, void* data, size_t length) {
  Wire.beginTransmission(EEPROM_ADDRESS);
  Wire.write((uint8_t)(address >> 8)); // MSB of address
  Wire.write((uint8_t)(address & 0xFF)); // LSB of address
  Wire.endTransmission();
  Wire.requestFrom(EEPROM_ADDRESS, length);
  TRACE("Requested: %d\n", length);
  uint8_t* byteData = (uint8_t*)data;
  while (Wire.available()) {
    byte datum = Wire.read();
    TRACE("C: %c", datum);
  }
  Serial.println();
  vTaskDelay(100 / portTICK_PERIOD_MS); // Delay to allow EEPROM write cycle to complete
  // if (Wire.available() >= length) {
  //   TRACE("DATA AVAILABLE\n");
  //   for (size_t i = 0; i < length; i++) {
  //     byteData[i] = Wire.read();
  //     TRACE("DATA: %s\n", byteData[i]);
  //   }
  // }
}
