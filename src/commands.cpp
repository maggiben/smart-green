#include <Arduino.h>
#include <string>
#include <ArduinoJson.h>
#include <RTClib.h> // Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include "settings.h"

// Forward declarations for handler functions
void handlePing(const String& command);
void handleBeep(const String& command);
void handleSetRTC(const String& isoDate);
void handleSetPlants(const String& jsonString);
void handleWater();
void handleWateringStatus();
void handlePlant();
void handleReadTask();
void handleResetTask();
void handleGetWateringTime();
void handleTime();
void handleAlarm();
void handlePlants();
void handleRestart();
void handleTriggerAlarm();
void handleLogs();
void handleNextAlarm();
void handleUnknown();

// Define a struct to hold command and function pointer
struct CommandHandler {
    String command;
    void (*handler)(const String&);
};

// Array of command handlers
CommandHandler commandHandlers[] = {
    {"ping", handlePing},
    {"beep", handleBeep},
    // Add other commands and handlers here
};

// Number of commands in the array
const int numCommands = sizeof(commandHandlers) / sizeof(commandHandlers[0]);

void handleCommand(const String& command) {
  // Trim and identify command type
  String cmd = command;
  cmd.trim();

  // Find command in array
  for (int i = 0; i < numCommands; i++) {
    if (cmd.startsWith(commandHandlers[i].command)) {
      // Call handler with remaining part of command
      commandHandlers[i].handler(cmd.substring(cmd.indexOf(':') + 1));
      return;
    }
  }
  // Handle unknown command
  handleUnknown();
}

void handlePing(const String& command) {
  Serial.println("pong!");
}

void handleBeep(const String& command) {
  beep(2, 150); // Assuming beep is a valid function
  Serial.println("beep!");
}

void handleUnknown() {
  Serial.println("Unknown input");
}
