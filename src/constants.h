// WiFi Credentials
#define WIFI_SSID                   "TP-Link_42B4"
#define WIFI_PASSWORD               "pirulo123"
#define HOSTNAME                    "indoor"
#define TIMEZONE                    "ART3ARST,M10.1.0/0,M3.3.0/0" // https://gist.github.com/tivaliy/a5ef9c7ccb4d57bdc248f0855c5eb1ff
#define MAX_I2C_DEVICES             128
#define I2C_MCP_PINCOUNT            12
#define EEPROM_ADDRESS              0x57
#define EEPROM_SIZE                 4096
#define EEPROM_SETTINGS_ADDRESS     0
#define MAX_PLANTS                  10  /* Maximun amount of allowed plants */
#define MAX_ALARMS                  10  /* Maximun amount of allowed alarms */
// TRACE output simplified, can be deactivated here
#define TRACE(...)                  Serial.printf(__VA_ARGS__)
#define PRINT(...)                  Serial.print(__VA_ARGS__)
#define PRINTLN(...)                Serial.println(__VA_ARGS__)
#define JSONBOOL(value)             value ? F("true") : F("false")
#define BUZZER_PIN                 32
#define PUMP1_PIN                  12
#define PUMP2_PIN                  13

