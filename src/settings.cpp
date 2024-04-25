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

