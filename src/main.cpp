#include "main.h"


void setupMcp() {
  // uncomment appropriate mcp.begin
  if (!mcp.begin_I2C()) {
    Serial.println("MCP I2c Error.");
  }
  for (int i = 0; i < 16; i++) {
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


  setupMcp();


  connectToWiFi(WIFI_SSID, WIFI_PASSWORD);
  
  if (isConnected()) {
    ip = WiFi.localIP();
    Serial.println();
    Serial.print("Connected: "); Serial.print(ip); Serial.println();
  } else {
    Serial.println();    
    errorMsg("Error connecting to WiFi");
  }

  // Ask for the current time using NTP request builtin into ESP firmware.
  TRACE("Setup ntp...\n");
  // initTime("WET0WEST,M3.5.0/1,M10.5.0");   // Set for Melbourne/AU
  initTime(TIMEZONE);   // Set for Melbourne/AU
  printLocalTime();
  // configTime(0, 0, "pool.ntp.com");

  // // Wait for time to synchronize
  // while (!time(nullptr)) {
  //   vTaskDelay(500 / portTICK_PERIOD_MS);
  //   TRACE("Waiting for time sync...");
  // }

  // struct tm timeinfo;
  // if(!getLocalTime(&timeinfo)){
  //   TRACE("Failed to obtain time\n");
  // }
  // setTimezone(TIMEZONE);  

  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");

  // Sync RTC
  // syncRTC();

  // time_t now = time(nullptr);
  // struct tm *timeinfo;
  // timeinfo = localtime(&now);

  // TRACE("Current time: %02d:%02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);


  // REST Endpoint
  server.on("/api/sysinfo", HTTP_GET, handleSysInfo);
  server.on("/api/valve", HTTP_POST, handleValve);
  // Enable CORS header in webserver results
  server.enableCORS(true);
  // Start Server
  server.begin();

  TRACE("open <http://%s> or <http://%s>\n", WiFi.getHostname(), WiFi.localIP().toString().c_str());
}

void setTimezone(String timezone) {
  TRACE(" Setting Timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(String timezone) {
  struct tm timeinfo;

  TRACE("Setting up time");
  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  if(!getLocalTime(&timeinfo)){
    TRACE("Failed to obtain time");
    return;
  }
  TRACE("Got the time from NTP");
  // Now we can set the real timezone
  setTimezone(timezone);
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
void printRtcTime() {
  DateTime now = rtc.now();

  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
}
void displayTime() {
  // text display tests
  DateTime now = rtc.now();
  char * time = (char *) malloc(sizeof(char) * 64);
  memset(time, 0, sizeof(char) * 64);
  sprintf(time, "Time: %02d:%02d:%02d", now.hour(), now.minute(), now.second() );
  display.clearDisplay();
  display.display();
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
  if (pinNumber >= 0 && pinNumber < 16) {
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
  WiFi.setHostname(HOSTNAME);
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
    Serial.print(".");
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
  rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                       timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
  Serial.println("RTC synced with NTP time");
}

// This function is called when the sysInfo service was requested.
void handleSysInfo() {
  String result;

  result += "{\n";
  result += "  \"Chip Model\": " + String(ESP.getChipModel()) + ",\n";
  result += "  \"Chip Cores\": " + String(ESP.getChipCores()) + ",\n";
  result += "  \"Chip Revision\": " + String(ESP.getChipRevision()) + ",\n";
  result += "  \"flashSize\": " + String(ESP.getFlashChipSize()) + ",\n";
  result += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
  // result += "  \"fsTotalBytes\": " + String(LittleFS.totalBytes()) + ",\n";
  // result += "  \"fsUsedBytes\": " + String(LittleFS.usedBytes()) + ",\n";
  result += "}";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/javascript; charset=utf-8", result);
}

void handleValve() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("plain") == false) {
      // handle error here
      server.send(400, "application/json", "{\"error\":\"Invalid value\"}");
    }
    JsonDocument json;
    DeserializationError error = deserializeJson(json, server.arg("plain"));
    
    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    int pin = json["pin"];
    String value = json["value"];

    if (value == "on" && pin >= 0 && pin <= 11) {
      turnOnPin(pin);
      server.send(200, "application/json", "{\"success\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Invalid value\"}");
    }
  } else {
    server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
  }
}

void errorMsg(String error, bool restart) {
  Serial.println(error);
  if (restart) {
    Serial.println("Rebooting now...");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP.restart();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  // printI2cDevices();
  // printRtcTime();
  // displayTime();
  // for (int i = 0; i < 16; i++) {
  //   turnOnPin(i);
  //   vTaskDelay(2000 / portTICK_PERIOD_MS);
  //   printRtcTime();
  //   displayTime();
  // }
  // vTaskDelay(500 / portTICK_PERIOD_MS);
  server.handleClient();
  // time_t now = time(nullptr);
  // struct tm *timeinfo;
  // timeinfo = localtime(&now);

  // TRACE("Current time: %02d:%02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  displayTime();
}
