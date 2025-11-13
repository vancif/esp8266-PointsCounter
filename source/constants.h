// ************************* CONSTANTS AND CONFIGURATION ******************************

// System Configuration
#define LOOP_DELAY 15  // milliseconds

// WiFi Configuration
#define AP_NAME "Points_Setup"
#define AP_PWD "Points123"
#define MAX_WIFI_NETWORKS 3
#define WIFI_SSID_LENGTH 32
#define WIFI_PWD_LENGTH 32
#define WIFI_TIMEOUT 15000 // 15 seconds

// Telnet Configuration
#define TELNET_PORT 23
#define TELNET_MAX_CLIENTS 1
#define TELNET_COMMAND_LENGTH 64

// Button Configuration
#define LONG_PRESS_TIME 350
#define BUTTON_DEBOUNCE 250

// Pin Definitions
#define BTN_MINUS D5
#define BTN_PLUS D6
#define BTN_MAIN D8

// Display Configuration
#define LCD_ADDRESS 0x27
#define LCD_COLS 20
#define LCD_ROWS 4
#define DISPLAY_UPDATE_INTERVAL 200  // milliseconds

// Game Configuration
#define MAX_PLAYERS 4
#define NAME_LENGTH 6
#define EEPROM_SIZE 512

// WiFi Storage Configuration
#define WIFI_EEPROM_START 256  // Start WiFi config after game data

// LED Control
#define LED_ON digitalWrite(LED_BUILTIN, LOW)
#define LED_OFF digitalWrite(LED_BUILTIN, HIGH)