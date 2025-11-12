#include <ArduinoOTA.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include "utils/customchars.h"
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <WiFiServer.h>

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

// ************************* ENUMS ******************************

enum DisplayState {
  STATE_NULL,
  STATE_MAIN,
  STATE_DETAIL,
  STATE_UTILS,
  STATE_CLOCK
};

// ************************* STRUCTS ******************************

struct WiFiConfig {
  char ssid[WIFI_SSID_LENGTH + 1];
  char password[WIFI_PWD_LENGTH + 1];
  bool active;
};

// ******************* GLOBAL VARIABLES ****************************

// Network components
ESP8266WiFiMulti wifiMulti;
ESP8266WebServer server(80);
IPAddress myIP;
WiFiConfig wifiConfigs[MAX_WIFI_NETWORKS];
uint8_t numWifiConfigs = 0;
bool isConfigMode = false;

// Telnet server components
WiFiServer telnetServer(TELNET_PORT);
WiFiClient telnetClients[TELNET_MAX_CLIENTS];
bool telnetConnected = false;

// Display components
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
char line[LCD_ROWS][LCD_COLS + 1];

// Game state
String playerName[MAX_PLAYERS];
uint8_t playerPoints[MAX_PLAYERS][MAX_PLAYERS];
uint8_t numPlayers = 0;
DisplayState displayState = STATE_NULL;

// UI state
uint8_t activeAction_Main = 0;
uint8_t activeAction_Detail = 0;
uint8_t activeAction_Utils = 0;
uint8_t activeAction_Clock = 0;
bool randomizeStart = false;

// Button state  
int lastButtonState = LOW;  // Start with LOW since we expect LOW with pull-down resistors
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;
unsigned long lastButtonRead = 0;
unsigned long lastButtonAction = 0;
int buttonDebounce = BUTTON_DEBOUNCE;

// Clock state
unsigned long p1_start_time = 0;
unsigned long p1_time = 0;
unsigned long p2_start_time = 0;
unsigned long p2_time = 0;
bool clockPaused = true;

// ************************* HTML STORED IN PROGMEM ******************************

const char GAME_HTML_PAGE[] PROGMEM = R"rawliteral(
<html>
<head><meta name="viewport" content="width=device-width, initial-scale=1.0" /></head>
<body>
<h2>Points Counter Game</h2>
<input maxlength="6" size="10" type="text" id="n0"><input maxlength="2" size="3" type="text" id="p0"><br><br>
<input maxlength="6" size="10" type="text" id="n1"><input maxlength="2" size="3" type="text" id="p1"><br><br>
<input maxlength="6" size="10" type="text" id="n2"><input maxlength="2" size="3" type="text" id="p2"><br><br>
<input maxlength="6" size="10" type="text" id="n3"><input maxlength="2" size="3" type="text" id="p3"><br><br>
<button onclick="send()">Update</button><br><br>
<button onclick="operate('1')" style="width:75px;height:75px;font:15pt Arial;">+</button>
<button onclick="operate('2')" style="width:75px;height:75px;font:15pt Arial;">-</button><br><br>
<button onclick="operate('3')" style="width:75px;height:75px;font:15pt Arial;">CH</button>
<button onclick="operate('4')" style="width:75px;height:75px;font:15pt Arial;">ENT</button>
<button onclick="operate('5')" style="width:75px;height:75px;font:15pt Arial;">OPT</button><br><br>
<a href="/wifi">WiFi Settings</a>
<script>
function operate(act){
  if(window.navigator.vibrate) window.navigator.vibrate(100);
  var data = new FormData();
  data.append("action", act);
  data.append("create", "false");
  fetch("/update", {method: "POST", body: data});
}
function send(){
  if(window.navigator.vibrate) window.navigator.vibrate(100);
  var data = new FormData();
  data.append("create", "true");
  for(let i = 0; i < 4; i++){
    data.append("n" + i, document.getElementById("n" + i).value);
    data.append("p" + i, document.getElementById("p" + i).value);
  }
  fetch("/update", {method: "POST", body: data});
}
</script>
</body>
</html>
)rawliteral";

const char WIFI_HTML_PAGE[] PROGMEM = R"rawliteral(
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>WiFi Configuration</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; }
    .wifi-item { background: #f0f0f0; margin: 10px 0; padding: 10px; border-radius: 5px; }
    .wifi-form { background: #e0e0e0; padding: 15px; border-radius: 5px; margin: 20px 0; }
    input[type="text"], input[type="password"] { width: 200px; padding: 5px; margin: 5px; }
    button { padding: 10px 15px; margin: 5px; }
    .delete-btn { background: #ff4444; color: white; }
  </style>
</head>
<body>
<h2>WiFi Configuration</h2>

<div id="availableNetworks">
  <h3>Available Networks:</h3>
  <!-- Networks will be populated here -->
  <button onclick="scanNetworks()">Scan for Networks</button>
</div>

<div id="savedNetworks">
  <h3>Saved Networks:</h3>
  <!-- Networks will be populated here -->
</div>

<div class="wifi-form">
  <h3>Add New Network:</h3>
  <form id="wifiForm">
    <label>SSID:</label><br>
    <input type="text" id="newSsid" name="ssid" maxlength="32" required><br>
    <label>Password:</label><br>
    <input type="password" id="newPassword" name="password" maxlength="32"><br><br>
    <button type="submit">Add Network</button>
  </form>
</div>

<div>
  <button onclick="window.location.href='/'">Back to Game</button>
  <button onclick="reboot()">Reboot Device</button>
</div>

<script>
// Load saved networks
function loadNetworks() {
  fetch('/wifi/list')
    .then(response => response.json())
    .then(data => {
      const container = document.getElementById('savedNetworks');
      let html = '<h3>Saved Networks:</h3>';
      
      if (data.networks && data.networks.length > 0) {
        data.networks.forEach((network, index) => {
          html += `
            <div class="wifi-item">
              <strong>${network.ssid}</strong>
              <button class="delete-btn" onclick="deleteNetwork(${index})">Delete</button>
            </div>`;
        });
      } else {
        html += '<p>No networks saved</p>';
      }
      container.innerHTML = html;
    });
}

// Add new network
document.getElementById('wifiForm').addEventListener('submit', function(e) {
  e.preventDefault();
  const ssid = document.getElementById('newSsid').value;
  const password = document.getElementById('newPassword').value;
  
  const data = new FormData();
  data.append('ssid', ssid);
  data.append('password', password);
  
  fetch('/wifi/add', {method: 'POST', body: data})
    .then(response => response.json())
    .then(result => {
      if (result.success) {
        document.getElementById('newSsid').value = '';
        document.getElementById('newPassword').value = '';
        loadNetworks();
        alert('Network added successfully!');
      } else {
        alert('Error: ' + result.message);
      }
    });
});

// Delete network
function deleteNetwork(index) {
  if (confirm('Are you sure you want to delete this network?')) {
    const data = new FormData();
    data.append('index', index);
    
    fetch('/wifi/delete', {method: 'POST', body: data})
      .then(response => response.json())
      .then(result => {
        if (result.success) {
          loadNetworks();
          alert('Network deleted successfully!');
        } else {
          alert('Error: ' + result.message);
        }
      });
  }
}

// populate network SSID on click
function populateNetwork(ssid) {
  document.getElementById('newSsid').value = ssid;
}

// Scan networks
function scanNetworks() {
  fetch('/wifi/scan')
    .then(response => response.json())
    .then(data => {
      let ssidList = 'Available Networks:<br>';
      data.networks.forEach(network => {
        ssidList += '<span>' + network.ssid + ' - RSSI: ' + network.rssi + 'dBm</span><button onclick="populateNetwork(\'' + network.ssid + '\')">Connect</button><br>';
      });
      document.getElementById('availableNetworks').innerHTML = '<h3>Available Networks:</h3><pre>' + ssidList + '</pre>';
    });
}

// Reboot device
function reboot() {
  if (confirm('Are you sure you want to reboot the device?')) {
    fetch('/reboot', {method: 'POST'})
      .then(() => {
        alert('Device rebooting... Please wait and reconnect.');
      });
  }
}

// Load networks on page load
loadNetworks();
</script>
</body>
</html>
)rawliteral";

// ************************* FUNCTION PROTOTYPES ******************************

// Setup functions
void initializeHardware();
void initializeDisplay();
void initializeWiFi();
void setupWebServer();
void setupTelnetServer();

// Telnet handling
void handleTelnetClients();
void processTelnetCommand(String command, WiFiClient& client);
void telnetPrint(const String& message);
void telnetPrintln(const String& message);
void showTelnetHelp(WiFiClient& client);
void showTelnetStatus(WiFiClient& client);

// Button handling
void buttonManagement();

// Display management
void printLines();
void updateDisplay();
void updateMainDisplay();
void updateDetailDisplay();
void updateUtilsDisplay();
void updateClockDisplay();
void clearDisplay();

// Data management
void saveData();
void loadData();
bool validateEEPROMData();
void saveWiFiConfig();
void loadWiFiConfig();
bool validateWiFiEEPROMData();
int addWiFiNetwork(const String& ssid, const String& password);
void deleteWiFiNetwork(uint8_t index);

// Web handlers
void handleRoot();
void handleGameSet();
void handleWiFiConfig();
void handleWiFiList();
void handleWiFiAdd();
void handleWiFiDelete();
void handleReboot();
void handleNotFound();
void handleWiFiScan();

// Button actions
void buttonChange();
void buttonEnter();
void buttonPlus();
void buttonMinus();
void buttonOption();

// Set button debounce time
void initializeButtonDebounce();

// OTA
void initializeOTA();

// Utility functions
void resetPlayerPoints(uint8_t startingPoints);

// ************************* UTILITY FUNCTIONS ******************************

void resetPlayerPoints(uint8_t startingPoints) {
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    playerPoints[i][0] = startingPoints;
    for (uint8_t j = 1; j < MAX_PLAYERS; j++) {
      playerPoints[i][j] = 0;
    }
  }
}

// ************************* DATA MANAGEMENT ******************************

void saveData() {
  uint16_t addr = 0;
  char nameBuffer[NAME_LENGTH + 1];

  // Save magic number for validation
  uint16_t magic = 0xFEDE;
  EEPROM.put(addr, magic);
  addr += sizeof(magic);

  // Save player names
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    playerName[i].toCharArray(nameBuffer, NAME_LENGTH + 1);
    EEPROM.put(addr, nameBuffer);
    addr += NAME_LENGTH + 1;
  }

  // Save points
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    for (uint8_t j = 0; j < MAX_PLAYERS; j++) {
      EEPROM.put(addr, playerPoints[i][j]);
      addr += sizeof(uint8_t);
    }
  }

  // Save number of players
  EEPROM.put(addr, numPlayers);

  EEPROM.commit();
  Serial.println(F("Data saved to EEPROM!"));
}

void loadData() {
  if (!validateEEPROMData()) {
    Serial.println(F("Invalid EEPROM data, using defaults"));
    return;
  }

  uint16_t addr = sizeof(uint16_t); // Skip magic number
  char nameBuffer[NAME_LENGTH + 1];

  // Load player names
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    EEPROM.get(addr, nameBuffer);
    nameBuffer[NAME_LENGTH] = '\0'; // Ensure null termination
    playerName[i] = String(nameBuffer);
    addr += NAME_LENGTH + 1;
  }

  // Load points
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    for (uint8_t j = 0; j < MAX_PLAYERS; j++) {
      EEPROM.get(addr, playerPoints[i][j]);
      addr += sizeof(uint8_t);
    }
  }

  // Load number of players
  EEPROM.get(addr, numPlayers);
  
  // Validate numPlayers
  if (numPlayers > MAX_PLAYERS) {
    numPlayers = MAX_PLAYERS;
  }

  Serial.println(F("Data loaded from EEPROM!"));
}

bool validateEEPROMData() {
  uint16_t magic;
  EEPROM.get(0, magic);
  return magic == 0xFEDE;
}

void saveWiFiConfig() {
  uint16_t addr = WIFI_EEPROM_START;
  
  // Save magic number for WiFi validation
  uint16_t wifiMagic = 0xC0DE;
  EEPROM.put(addr, wifiMagic);
  addr += sizeof(wifiMagic);
  
  // Save number of WiFi configs
  EEPROM.put(addr, numWifiConfigs);
  addr += sizeof(numWifiConfigs);
  
  // Save WiFi configurations
  for (uint8_t i = 0; i < MAX_WIFI_NETWORKS; i++) {
    EEPROM.put(addr, wifiConfigs[i]);
    addr += sizeof(WiFiConfig);
  }
  
  EEPROM.commit();
  Serial.println(F("WiFi config saved to EEPROM!"));
}

void loadWiFiConfig() {
  if (!validateWiFiEEPROMData()) {
    Serial.println(F("Invalid WiFi EEPROM data, using defaults"));
    numWifiConfigs = 0;
    return;
  }
  
  uint16_t addr = WIFI_EEPROM_START + sizeof(uint16_t); // Skip magic number
  
  // Load number of WiFi configs
  EEPROM.get(addr, numWifiConfigs);
  addr += sizeof(numWifiConfigs);
  
  // Validate numWifiConfigs
  if (numWifiConfigs > MAX_WIFI_NETWORKS) {
    numWifiConfigs = MAX_WIFI_NETWORKS;
  }
  
  // Load WiFi configurations
  for (uint8_t i = 0; i < MAX_WIFI_NETWORKS; i++) {
    EEPROM.get(addr, wifiConfigs[i]);
    addr += sizeof(WiFiConfig);
  }
  
  Serial.print(F("WiFi config loaded! Networks: "));
  Serial.println(numWifiConfigs);
}

bool validateWiFiEEPROMData() {
  uint16_t wifiMagic;
  EEPROM.get(WIFI_EEPROM_START, wifiMagic);
  return wifiMagic == 0xC0DE;
}

int addWiFiNetwork(const String& ssid, const String& password) {
  if (numWifiConfigs >= MAX_WIFI_NETWORKS) {
    Serial.println(F("Maximum WiFi networks reached"));
    return 1;
  }
  
  // Check if SSID already exists
  for (uint8_t i = 0; i < numWifiConfigs; i++) {
    if (String(wifiConfigs[i].ssid) == ssid) {
      // Update existing network
      strncpy(wifiConfigs[i].password, password.c_str(), WIFI_PWD_LENGTH);
      wifiConfigs[i].password[WIFI_PWD_LENGTH] = '\0';
      wifiConfigs[i].active = true;
      saveWiFiConfig();
      Serial.println(F("WiFi network updated"));
      return 0;
    }
  }
  
  // Add new network
  strncpy(wifiConfigs[numWifiConfigs].ssid, ssid.c_str(), WIFI_SSID_LENGTH);
  wifiConfigs[numWifiConfigs].ssid[WIFI_SSID_LENGTH] = '\0';
  strncpy(wifiConfigs[numWifiConfigs].password, password.c_str(), WIFI_PWD_LENGTH);
  wifiConfigs[numWifiConfigs].password[WIFI_PWD_LENGTH] = '\0';
  wifiConfigs[numWifiConfigs].active = true;
  
  numWifiConfigs++;
  saveWiFiConfig();
  Serial.println(F("WiFi network added"));
  return 0;
}

void deleteWiFiNetwork(uint8_t index) {
  if (index >= numWifiConfigs) {
    Serial.println(F("Invalid WiFi network index"));
    return;
  }
  
  // Shift remaining networks down
  for (uint8_t i = index; i < numWifiConfigs - 1; i++) {
    wifiConfigs[i] = wifiConfigs[i + 1];
  }
  
  numWifiConfigs--;
  
  // Clear the last slot
  memset(&wifiConfigs[numWifiConfigs], 0, sizeof(WiFiConfig));
  
  saveWiFiConfig();
  Serial.println(F("WiFi network deleted"));
}

// ************************* HARDWARE INITIALIZATION ******************************

void initializeHardware() {
  // Initialize LED
  pinMode(LED_BUILTIN, OUTPUT);
  LED_OFF;

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Initialize buttons with external pull-down resistors (ESP8266 doesn't have INPUT_PULLDOWN)
  pinMode(BTN_MINUS, INPUT);
  pinMode(BTN_PLUS, INPUT);
  pinMode(BTN_MAIN, INPUT);

  // Initialize serial
  Serial.begin(115200);
  Serial.println(F("\nESP8266 Points Counter Starting..."));
}

void initializeDisplay() {
  lcd.init();
  lcd.backlight();
  
  // Force complete LCD clear
  lcd.clear();
  delay(100);
  
  // Load custom characters using the utility function
  initializeCustomCharacters(lcd);

  // Clear all line buffers
  for (uint8_t i = 0; i < LCD_ROWS; i++) {
    snprintf(line[i], LCD_COLS + 1, "%*s", LCD_COLS, "");
  }
  
  // Show boot message using line buffers
  snprintf(line[0], LCD_COLS + 1, "ESP8266 Points");
  snprintf(line[1], LCD_COLS + 1, "Counter v2.0");  
  snprintf(line[2], LCD_COLS + 1, "Booting up...");
  printLines();
  delay(1000);
}

// ************************* WIFI INITIALIZATION ******************************

void initializeWiFi() {
  Serial.println(F("WiFi Connecting..."));
  
  // Load WiFi configuration from EEPROM
  loadWiFiConfig();

  // Clear display and show WiFi setup message
  clearDisplay();
  snprintf(line[3], LCD_COLS + 1, "WiFi setup...");
  printLines();

  // Add saved Wi-Fi networks
  bool hasNetworks = false;
  for (uint8_t i = 0; i < numWifiConfigs; i++) {
    if (wifiConfigs[i].active && strlen(wifiConfigs[i].ssid) > 0) {
      wifiMulti.addAP(wifiConfigs[i].ssid, wifiConfigs[i].password);
      hasNetworks = true;
      Serial.print(F("Added WiFi: "));
      Serial.println(wifiConfigs[i].ssid);
    }
  }
  
  unsigned long wifiStartTime = millis();
  bool wifiConnected = false;
  
  // Attempt to connect to WiFi if we have networks
  if (hasNetworks) {
    while (millis() - wifiStartTime < WIFI_TIMEOUT && !wifiConnected) {
      if (wifiMulti.run() == WL_CONNECTED) {
        wifiConnected = true;
        break;
      }
      delay(250);
      Serial.print('.');
    }
  }
  
  if (wifiConnected) {
    // Successfully connected to WiFi
    myIP = WiFi.localIP();
    isConfigMode = false;
    
    Serial.println();
    Serial.print(F("Connected to: "));
    Serial.println(WiFi.SSID());
    Serial.print(F("IP address: "));
    Serial.println(myIP);
    
    // Clear display and show temporary mDNS setup message
    clearDisplay();
    snprintf(line[2], LCD_COLS + 1, "%.19s", myIP.toString().c_str());
    snprintf(line[3], LCD_COLS + 1, "mDNS setup...");
    printLines();
    delay(1000);
    
    // Start mDNS responder
    if (MDNS.begin("MagicPoints")) {
      Serial.println(F("mDNS responder started"));
    } else {
      Serial.println(F("Error setting up MDNS responder!"));
    }
    
    // Clear and prepare final WiFi display messages
    clearDisplay();
    snprintf(line[0], LCD_COLS + 1, "Connect to this wifi");
    snprintf(line[1], LCD_COLS + 1, "to start a game");
    snprintf(line[2], LCD_COLS + 1, "SSID: %.13s", WiFi.SSID().c_str());
    snprintf(line[3], LCD_COLS + 1, "IP: %.15s", myIP.toString().c_str());
    
  } else {
    // WiFi connection failed or no networks saved, start Access Point for configuration
    Serial.println();
    Serial.println(F("Starting WiFi configuration mode"));
    
    isConfigMode = true;
    
    // Show temporary status
    clearDisplay();
    snprintf(line[2], LCD_COLS + 1, "Config mode...");
    snprintf(line[3], LCD_COLS + 1, "Starting AP...");
    printLines();
    delay(1000);
    
    // Optimize AP settings for better performance
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    
    // Use channel 6 (less congested) and set max connections to 4
    bool apStarted = WiFi.softAP(AP_NAME, AP_PWD, 6, 0, 4);
    
    if (apStarted) {
      myIP = WiFi.softAPIP();
      
      Serial.print(F("AP started: "));
      Serial.println(AP_NAME);
      Serial.print(F("AP IP: "));
      Serial.println(myIP);
      
      // Set WiFi power to maximum for better range in AP mode
      WiFi.setOutputPower(20.5); // Max power in dBm
      
    } else {
      Serial.println(F("Failed to start AP!"));
      // Fallback to default settings
      WiFi.softAP(AP_NAME, AP_PWD);
      myIP = WiFi.softAPIP();
    }
    
    // Clear and prepare final AP display messages
    clearDisplay();
    snprintf(line[0], LCD_COLS + 1, "WiFi Setup Mode");
    snprintf(line[1], LCD_COLS + 1, "SSID: %.13s", AP_NAME);
    snprintf(line[2], LCD_COLS + 1, "PWD:  %.13s", AP_PWD);
    snprintf(line[3], LCD_COLS + 1, "IP: %.15s", myIP.toString().c_str());
    // Not printing lines to display since we have other things to do after this
    // before user can interact.
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_POST, handleGameSet);
  server.on("/wifi", HTTP_GET, handleWiFiConfig);
  server.on("/wifi/list", HTTP_GET, handleWiFiList);
  server.on("/wifi/add", HTTP_POST, handleWiFiAdd);
  server.on("/wifi/delete", HTTP_POST, handleWiFiDelete);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/wifi/scan", HTTP_GET, handleWiFiScan);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println(F("HTTP server started"));
}

// ************************* TELNET SERVER SETUP ******************************

void setupTelnetServer() {
  telnetServer.begin();
  telnetServer.setNoDelay(true);
  
  Serial.print(F("Telnet server started on port "));
  Serial.println(TELNET_PORT);
  Serial.println(F("Connect using: telnet <IP> 23"));
}

// ************************* BUTTON HANDLING ******************************

void buttonManagement() {
  unsigned long currentTime = millis();
  
  // Debounce - prevent actions too close together
  if (currentTime - lastButtonAction < buttonDebounce) {
    return;
  }
  
  // ********* D8 LONG-SHORT PRESS *********
  int currentState = digitalRead(BTN_MAIN);

  if (lastButtonState == LOW && currentState == HIGH)       // button is pressed
    pressedTime = millis();
  else if (lastButtonState == HIGH && currentState == LOW) { // button is released
    releasedTime = millis();

    long pressDuration = releasedTime - pressedTime;

    if ( pressDuration < LONG_PRESS_TIME ) {
      // SHORT PRESS
      buttonChange();
      lastButtonAction = currentTime;
    } else if ( pressDuration >= LONG_PRESS_TIME ) {
      // LONG PRESS
      buttonEnter();
      lastButtonAction = currentTime;
    }
  }

  // save the last state
  lastButtonState = currentState;

  //************* OTHER SIMPLE BUTTONS *********
  if (digitalRead(BTN_MINUS) == HIGH && digitalRead(BTN_PLUS) == LOW) {
    buttonMinus();
    lastButtonAction = currentTime;
  }
  else if (digitalRead(BTN_PLUS) == HIGH && digitalRead(BTN_MINUS) == LOW) {
    buttonPlus();
    lastButtonAction = currentTime;
  }
  else if (digitalRead(BTN_PLUS) == HIGH && digitalRead(BTN_MINUS) == HIGH) {
    buttonOption();
    lastButtonAction = currentTime;
  }
}

void initializeButtonDebounce() {
  uint16_t debounceMagic;
  EEPROM.get(234, debounceMagic);
  if (debounceMagic != 0xDBC0) {
    // First time setup, write default debounce value
    EEPROM.put(234, (uint16_t)0xDBC0);
    EEPROM.put(230, (int)BUTTON_DEBOUNCE);
    EEPROM.commit();
    buttonDebounce = BUTTON_DEBOUNCE;
    Serial.print(F("Button debounce time set to default: "));
    Serial.print(buttonDebounce);
  } else {
    EEPROM.get(230, buttonDebounce);
    Serial.print(F("Button debounce time: "));
    Serial.print(buttonDebounce);
  }
}

// ************************* DISPLAY MANAGEMENT ******************************

void clearDisplay() {
  for (uint8_t i = 0; i < LCD_ROWS; i++) {
    snprintf(line[i], LCD_COLS + 1, "%*s", LCD_COLS, ""); // Fill with spaces
  }
  printLines();
}

void printLines() {
  for (uint8_t i = 0; i < LCD_ROWS; i++) {
    lcd.setCursor(0, i);
    lcd.print(line[i]);
  }
}

void updateDisplay() {
  switch (displayState) {
    case STATE_MAIN:
      updateMainDisplay();
      break;
    case STATE_DETAIL:
      updateDetailDisplay();
      break;
    case STATE_UTILS:
      updateUtilsDisplay();
      break;
    case STATE_CLOCK:
      updateClockDisplay();
      break;
    case STATE_NULL:
    default:
      // Display connection info or default state
      break;
  }
  printLines();
}

void updateMainDisplay() {
  // Clear all lines first
  // not using clearDisplay() to avoid unnecessary screen cleaning flicker
  for (uint8_t i = 0; i < LCD_ROWS; i++) {
    snprintf(line[i], LCD_COLS + 1, "%*s", LCD_COLS, ""); // Fill with spaces
  }
  
  // Display player information
  for (uint8_t i = 0; i < LCD_ROWS; i++) {
    if (i < numPlayers) {
      snprintf(line[i], LCD_COLS + 1, " %-6s: %2d         ", 
               playerName[i].c_str(), playerPoints[i][0]);
    }
  }
  
  // Add "Utils" text to bottom right
  if (LCD_ROWS >= 4) {
    strncpy(&line[3][14], "Utils", 5);
  }
  
  // Handle randomize animation
  if (randomizeStart && random(0, 11) <= 9) {
    activeAction_Main = (activeAction_Main + 1) % numPlayers;
    line[0][19] = CHAR_HOURGLASS;
    delay(250);
  } else {
    randomizeStart = false;
  }
  
  // Add selection indicators
  if (activeAction_Main < MAX_PLAYERS) {
    line[activeAction_Main][0] = CHAR_RIGHT_ARROW;
  } else if (activeAction_Main == MAX_PLAYERS) {
    line[3][19] = CHAR_LEFT_ARROW;
  }
}

void updateDetailDisplay() {
  snprintf(line[0], LCD_COLS + 1, "Comm.Dmg %-6s P:%2d", 
           playerName[activeAction_Main].c_str(), playerPoints[activeAction_Main][0]);

  for (uint8_t i = 1; i < LCD_ROWS; i++) {
    uint8_t targetPlayer = (activeAction_Main + i) % MAX_PLAYERS;
    snprintf(line[i], LCD_COLS + 1, "%-6s: %2d          ", 
             playerName[targetPlayer].c_str(), playerPoints[activeAction_Main][i]);
  }
  
  // Add selection indicator
  if (activeAction_Detail >= 1 && activeAction_Detail <= 3) {
    line[activeAction_Detail][11] = CHAR_LEFT_ARROW;
  }
}

void updateUtilsDisplay() {
  snprintf(line[0], LCD_COLS + 1, " Die Roll      SAVE ");
  snprintf(line[1], LCD_COLS + 1, " Reset 20      LOAD ");
  snprintf(line[2], LCD_COLS + 1, " Reset 40     CLOCK ");
  snprintf(line[3], LCD_COLS + 1, "        BACK        ");
  
  // Add selection indicators
  switch (activeAction_Utils) {
    case 0: line[0][0] = CHAR_RIGHT_ARROW; break;
    case 1: line[1][0] = CHAR_RIGHT_ARROW; break;
    case 2: line[2][0] = CHAR_RIGHT_ARROW; break;
    case 3: line[0][19] = CHAR_LEFT_ARROW; break;
    case 4: line[1][19] = CHAR_LEFT_ARROW; break;
    case 5: line[2][19] = CHAR_LEFT_ARROW; break;
    case 6: line[3][12] = CHAR_LEFT_ARROW; break;
  }
}

void updateClockDisplay() {
  unsigned long now = millis();
  
  // Update running timer for active player
  if (!clockPaused) {
    if (activeAction_Clock == 1 && p1_start_time > 0) {
      p1_time += (now - p1_start_time);
      p1_start_time = now;
    } else if (activeAction_Clock == 2 && p2_start_time > 0) {
      p2_time += (now - p2_start_time);
      p2_start_time = now;
    }
  }
  
  // Format time display
  unsigned long totalTime = p1_time + p2_time;
  
  snprintf(line[0], LCD_COLS + 1, "       CLOCK        ");
  snprintf(line[1], LCD_COLS + 1, "P1 %02lu:%02lu    P2 %02lu:%02lu",
           p1_time / 60000, (p1_time % 60000) / 1000,
           p2_time / 60000, (p2_time % 60000) / 1000);
  snprintf(line[2], LCD_COLS + 1, "                    ");
  snprintf(line[3], LCD_COLS + 1, "    Total  %02lu:%02lu    ",
           totalTime / 60000, (totalTime % 60000) / 1000);
  
  // Add selection indicators
  switch (activeAction_Clock) {
    case 1: line[1][8] = CHAR_LEFT_ARROW; break;
    case 2: line[1][11] = CHAR_RIGHT_ARROW; break;
  }
  
  if (clockPaused) {
    line[0][19] = CHAR_HOURGLASS;
  }
}

// ************************* WEB HANDLERS ******************************

void handleRoot() {
  LED_ON;
  server.send_P(200, "text/html", GAME_HTML_PAGE);
  yield(); // Allow WiFi stack to process
  LED_OFF;
}

void handleWiFiConfig() {
  LED_ON;
  server.send_P(200, "text/html", WIFI_HTML_PAGE);
  yield(); // Allow WiFi stack to process
  LED_OFF;
}

void handleWiFiList() {
  LED_ON;
  
  String json;
  json.reserve(200); // Pre-allocate memory to prevent fragmentation
  json = "{\"networks\":[";
  
  for (uint8_t i = 0; i < numWifiConfigs; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + String(wifiConfigs[i].ssid) + "\"}";
  }
  
  json += "]}";
  
  server.send(200, "application/json", json);
  yield(); // Allow WiFi stack to process
  LED_OFF;
}

void handleWiFiAdd() {
  LED_ON;
  
  String response;
  
  if (server.hasArg("ssid")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    if (ssid.length() > 0 && ssid.length() <= WIFI_SSID_LENGTH) {
      uint8_t result = addWiFiNetwork(ssid, password);
      if (result == 0) {
        response = "{\"success\":true,\"message\":\"Network added successfully\"}";
      } else {
        response = "{\"success\":false,\"message\":\"Failed to add network\"}";
      }
    } else {
      response = "{\"success\":false,\"message\":\"Invalid SSID length\"}";
    }
  } else {
    response = "{\"success\":false,\"message\":\"Missing SSID parameter\"}";
  }
  
  server.send(200, "application/json", response);
  yield(); // Allow WiFi stack to process
  LED_OFF;
}

void handleWiFiDelete() {
  LED_ON;
  
  String response;
  
  if (server.hasArg("index")) {
    int index = server.arg("index").toInt();
    
    if (index >= 0 && index < numWifiConfigs) {
      deleteWiFiNetwork(index);
      response = "{\"success\":true,\"message\":\"Network deleted successfully\"}";
    } else {
      response = "{\"success\":false,\"message\":\"Invalid network index\"}";
    }
  } else {
    response = "{\"success\":false,\"message\":\"Missing index parameter\"}";
  }
  
  server.send(200, "application/json", response);
  yield(); // Allow WiFi stack to process
  LED_OFF;
}

void handleReboot() {
  LED_ON;
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
  LED_OFF;
  
  delay(1000);
  ESP.restart();
}

void handleGameSet() {
  LED_ON;
  
  if (server.arg("create") == "true") {
    displayState = STATE_MAIN;
    numPlayers = 0;
    
    // Process player data from web form
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
      String nameArg = "n" + String(i);
      String pointsArg = "p" + String(i);
      
      String name = server.arg(nameArg);
      String points = server.arg(pointsArg);
      
      if (name.length() > 0) {
        numPlayers++;
        // Replace special characters with custom LCD characters
        name.replace("à", String((char)CHAR_A_ACCENT)); // Custom character 4 for à
        name.replace("è", String((char)CHAR_A_ACCENT)); // Custom character 5 for è
        playerName[i] = name.substring(0, NAME_LENGTH); // Limit name length
        playerPoints[i][0] = constrain(points.toInt(), 0, 255); // Limit points range
      } else {
        playerName[i] = "";
        playerPoints[i][0] = 0;
      }
      
      // Clear other point categories
      for (uint8_t j = 1; j < MAX_PLAYERS; j++) {
        playerPoints[i][j] = 0;
      }
      
      // Yield periodically
      if (i % 2 == 0) yield();
    }
    
    Serial.print(F("Created game with "));
    Serial.print(numPlayers);
    Serial.println(F(" players"));
    
  } else if (server.hasArg("action")) {
    int action = server.arg("action").toInt();
    switch (action) {
      case 1: buttonPlus(); break;
      case 2: buttonMinus(); break;
      case 3: buttonChange(); break;
      case 4: buttonEnter(); break;
      case 5: buttonOption(); break;
      default:
        Serial.println(F("Unknown web action"));
    }
  }
  
  // Send minimal response quickly
  server.send(200, "application/json", "{\"ok\":1}");
  yield(); // Allow WiFi stack to process
  LED_OFF;
}

void handleNotFound() {
  LED_ON;
  server.send(404, "text/plain", "404: Not found");
  LED_OFF;
}

void handleWiFiScan() {
  LED_ON;
  
  // Limit scan time to prevent blocking
  WiFi.scanDelete(); // Clear previous results
  yield();
  
  int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true
  yield();
  
  // Build response efficiently
  String json;
  json.reserve(512); // Pre-allocate memory
  json = "{\"status\":\"success\",\"networks\":[";
  
  for (int i = 0; i < n && i < 20; ++i) { // Limit to 20 networks max
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + WiFi.RSSI(i) + "}";
    
    // Yield periodically during long operations
    if (i % 5 == 0) yield();
  }
  json += "]}";
  
  server.send(200, "application/json", json);
  yield();
  LED_OFF;
}

// ************************* TELNET HANDLERS ******************************

void handleTelnetClients() {
  // Check for new clients
  if (telnetServer.hasClient()) {
    // Find a free slot for the new client
    for (uint8_t i = 0; i < TELNET_MAX_CLIENTS; i++) {
      if (!telnetClients[i] || !telnetClients[i].connected()) {
        if (telnetClients[i]) telnetClients[i].stop();
        telnetClients[i] = telnetServer.available();
        
        Serial.print(F("New Telnet client connected from: "));
        Serial.println(telnetClients[i].remoteIP());
        
        telnetClients[i].println(F("===================================="));
        telnetClients[i].println(F("ESP8266 Points Counter - Telnet"));
        telnetClients[i].println(F("===================================="));
        telnetClients[i].println(F("Type 'help' for available commands"));
        telnetClients[i].print(F("> "));
        
        telnetConnected = true;
        break;
      }
    }
    
    // If no free slot, reject the connection
    if (telnetServer.hasClient()) {
      WiFiClient rejectClient = telnetServer.available();
      rejectClient.println(F("Server busy. Connection rejected."));
      rejectClient.stop();
    }
  }
  
  // Handle existing clients
  for (uint8_t i = 0; i < TELNET_MAX_CLIENTS; i++) {
    if (telnetClients[i] && telnetClients[i].connected()) {
      if (telnetClients[i].available()) {
        String command = telnetClients[i].readStringUntil('\n');
        command.trim();
        
        if (command.length() > 0) {
          processTelnetCommand(command, telnetClients[i]);
        }
        telnetClients[i].print(F("> "));
      }
    } else if (telnetClients[i]) {
      // Cleanup disconnected clients
      telnetClients[i].stop();
      Serial.print(F("Telnet client "));
      Serial.print(i);
      Serial.println(F(" disconnected"));
    }
  }
  
  // Update connection status
  telnetConnected = false;
  for (uint8_t i = 0; i < TELNET_MAX_CLIENTS; i++) {
    if (telnetClients[i] && telnetClients[i].connected()) {
      telnetConnected = true;
      break;
    }
  }
}

void processTelnetCommand(String command, WiFiClient& client) {
  command.toLowerCase();
  
  if (command == "help") {
    showTelnetHelp(client);
  } else if (command == "status") {
    showTelnetStatus(client);
  } else if (command == "players") {
    client.println(F("Current Players:"));
    for (uint8_t i = 0; i < numPlayers; i++) {
      client.print(F("Player "));
      client.print(i + 1);
      client.print(F(": "));
      client.print(playerName[i]);
      client.print(F(" (Points: "));
      client.print(playerPoints[i][0]);
      client.println(F(")"));
    }
  } else if (command.startsWith("points ")) {
    // Format: points <player_num> <new_points>
    int firstSpace = command.indexOf(' ');
    int secondSpace = command.indexOf(' ', firstSpace + 1);
    
    if (firstSpace != -1 && secondSpace != -1) {
      uint8_t playerNum = command.substring(firstSpace + 1, secondSpace).toInt();
      uint8_t newPoints = command.substring(secondSpace + 1).toInt();
      
      if (playerNum >= 1 && playerNum <= numPlayers) {
        playerPoints[playerNum - 1][0] = constrain(newPoints, 0, 255);
        client.print(F("Set player "));
        client.print(playerNum);
        client.print(F(" points to "));
        client.println(newPoints);
      } else {
        client.println(F("Invalid player number"));
      }
    } else {
      client.println(F("Usage: points <player_num> <new_points>"));
    }
  } else if (command == "save") {
    saveData();
  } else if (command == "load") {
    loadData();
  } else if (command == "reset20") {
    resetPlayerPoints(20);
    client.println(F("All players reset to 20 points"));
  } else if (command == "reset40") {
    resetPlayerPoints(40);
    client.println(F("All players reset to 40 points"));
  } else if (command == "wifi") {
    client.print(F("WiFi Status: "));
    client.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    if (WiFi.status() == WL_CONNECTED) {
      client.print(F("SSID: "));
      client.println(WiFi.SSID());
      client.print(F("IP: "));
      client.println(WiFi.localIP());
      client.print(F("Signal: "));
      client.print(WiFi.RSSI());
      client.println(F(" dBm"));
    }
  } else if (command == "memory") {
    client.print(F("Free Heap: "));
    client.print(ESP.getFreeHeap());
    client.println(F(" bytes"));
    client.print(F("Heap Fragmentation: "));
    client.print(ESP.getHeapFragmentation());
    client.println(F("%"));
    client.print(F("Max Free Block: "));
    client.print(ESP.getMaxFreeBlockSize());
    client.println(F(" bytes"));
  } else if (command == "uptime") {
    unsigned long uptime = millis();
    unsigned long days = uptime / 86400000;
    unsigned long hours = (uptime % 86400000) / 3600000;
    unsigned long minutes = (uptime % 3600000) / 60000;
    unsigned long seconds = (uptime % 60000) / 1000;
    
    client.print(F("Uptime: "));
    client.print(days);
    client.print(F(" days, "));
    client.print(hours);
    client.print(F(" hours, "));
    client.print(minutes);
    client.print(F(" minutes, "));
    client.print(seconds);
    client.println(F(" seconds"));
  } else if (command == "reboot") {
    client.println(F("Rebooting device..."));
    delay(1000);
    ESP.restart();
  } else if (command == "display") {
    client.print(F("Display State: "));
    switch (displayState) {
      case STATE_NULL: client.println(F("NULL")); break;
      case STATE_MAIN: client.println(F("MAIN")); break;
      case STATE_DETAIL: client.println(F("DETAIL")); break;
      case STATE_UTILS: client.println(F("UTILS")); break;
      case STATE_CLOCK: client.println(F("CLOCK")); break;
      default: client.println(F("UNKNOWN")); break;
    }
  } else if (command == "main") {
    displayState = STATE_MAIN;
    activeAction_Main = 0;
    client.println(F("Switched to main display"));
  } else if (command == "plus") {
    buttonPlus();
    client.println(F("Plus button pressed"));
  } else if (command == "minus") {
    buttonMinus();
    client.println(F("Minus button pressed"));
  } else if (command == "enter") {
    buttonEnter();
    client.println(F("Enter button pressed"));
  } else if (command == "change") {
    buttonChange();
    client.println(F("Change button pressed"));
  } else if (command == "quit" || command == "exit") {
    client.println(F("Goodbye!"));
    client.stop();
  } else if (command == "wifilist" ) {
    client.println(F("Saved WiFi Networks:"));
    for (uint8_t i = 0; i < MAX_WIFI_NETWORKS; i++) {
      client.print(i);
      client.print(F(": "));
      client.println(wifiConfigs[i].ssid);
    }
  } else if ( command.startsWith("eeprom write") ) {
    // Format: write eeprom <address> <value>
    // Value is a single byte (0-255) - supports decimal and hex (0x prefix)
    int firstSpace = command.indexOf(' ', 14);  // Find space after address
    int addr = command.substring(13, firstSpace).toInt();

    if (firstSpace == -1) {
      client.println(F("Usage: eeprom write <address> <value>"));
      return;
    }
    if (addr < 0 || addr >= EEPROM_SIZE) {
      client.println(F("Invalid EEPROM address"));
      return;
    }

    String valueStr = command.substring(firstSpace + 1);
    uint8_t value;
    
    if (valueStr.startsWith("0x") || valueStr.startsWith("0X")) {
      // Parse as hexadecimal
      value = strtol(valueStr.c_str(), NULL, 16);
    } else {
      // Parse as decimal
      value = valueStr.toInt();
    }

    EEPROM.write(addr, value);
    EEPROM.commit();
    
    client.println();
    client.print(F("Successfully wrote "));
    client.print(value);
    client.print(F(" (0x"));
    if (value < 16) client.print(F("0"));
    client.print(value, HEX);
    client.print(F(") to address "));
    client.println(addr);
  } else if ( command == "eeprom read all") {
    // let's read all EEPROM
    client.println(F("EEPROM Contents:"));
    for (int addr = 0; addr < EEPROM_SIZE; addr++) {
      char value = EEPROM.read(addr);
      client.print(F("Addr "));
      client.print(addr);
      client.print(F(": '"));
      if (isPrintable(value)) {
        client.print(value);
      } else {
        client.print(F("."));
      }
      client.print(F("' ("));
      client.print((uint8_t)value);
      client.print(F(") - 0x"));
      // Print hex value with leading zero if needed
      if ((uint8_t)value < 16) {
        client.print(F("0"));
      }
      client.print((uint8_t)value, HEX);
      client.println();
    }
    
  } else if (command.startsWith("eeprom read byte")) {
    int firstSpace = command.indexOf(' ', 17);  // Find space after address
    int addr = command.substring(16, firstSpace).toInt();
    char value = EEPROM.read(addr);
    client.print(F("Addr "));
    client.print(addr);
    client.print(F(": '"));
    client.print(F("0x"));
    // Print hex value with leading zero if needed
    if ((uint8_t)value < 16) {
      client.print(F("0"));
    }
    client.print((uint8_t)value, HEX);
    client.println();
  } else if (command.startsWith("debounce set")) {
    int firstSpace = command.indexOf(' ', 12);  // Find space after value
    if (firstSpace == -1) {
      client.println(F("Usage: debounce set <milliseconds>"));
      return;
    }
    int debounceTime = command.substring(firstSpace + 1).toInt();
    if (debounceTime < 0 || debounceTime > 5000) {
      client.println(F("Invalid debounce time (0-5000 ms)"));
      return;
    }
    buttonDebounce = debounceTime;
    EEPROM.put(230, buttonDebounce);
    EEPROM.commit();
    client.print(F("Button debounce time set to "));
    client.print(buttonDebounce);
    client.println(F(" ms"));

  } else if (command.length() > 0) {
    client.print(F("Unknown command: "));
    client.println(command);
    client.println(F("Type 'help' for available commands"));
  }
}

void showTelnetHelp(WiFiClient& client) {
  client.println(F("Available Commands:"));
  client.println(F("=================="));
  client.println(F("help                      - Show this help"));
  client.println(F("status                    - Show system status"));
  client.println(F("players                   - List all players and points"));
  client.println(F("points <p> <n>            - Set player <p> to <n> points"));
  client.println(F("save                      - Save game data"));
  client.println(F("load                      - Load game data"));
  client.println(F("reset20                   - Reset all players to 20 points"));
  client.println(F("reset40                   - Reset all players to 40 points"));
  client.println(F("wifi                      - Show WiFi status"));
  client.println(F("wifilist                  - List saved WiFi networks"));
  client.println(F("memory                    - Show memory info"));
  client.println(F("uptime                    - Show device uptime"));
  client.println(F("display                   - Show current display state"));
  client.println(F("main                      - Switch to main display"));
  client.println(F("plus                      - Simulate plus button"));
  client.println(F("minus                     - Simulate minus button"));
  client.println(F("enter                     - Simulate enter button"));
  client.println(F("change                    - Simulate change button"));
  client.println(F("eeprom write <a> <v>      - Write value <v> (dec/hex) to EEPROM address <a>"));
  client.println(F("eeprom read all           - Read all EEPROM contents"));
  client.println(F("eeprom read byte <a>      - Read byte from EEPROM address <a>"));
  client.println(F("debounce set <ms>         - Set button debounce time (0-5000 ms)"));
  client.println(F("reboot                    - Restart the device"));
  client.println(F("quit/exit                 - Close Telnet connection"));
}

void showTelnetStatus(WiFiClient& client) {
  client.println(F("System Status:"));
  client.println(F("=============="));
  
  client.print(F("Device: ESP8266 Points Counter v2.0"));
  client.println();
  
  client.print(F("Uptime: "));
  unsigned long uptime = millis();
  client.print(uptime / 1000);
  client.println(F(" seconds"));
  
  client.print(F("Free Memory: "));
  client.print(ESP.getFreeHeap());
  client.println(F(" bytes"));
  
  client.print(F("WiFi: "));
  if (WiFi.status() == WL_CONNECTED) {
    client.print(F("Connected to "));
    client.println(WiFi.SSID());
    client.print(F("IP: "));
    client.println(WiFi.localIP());
  } else {
    client.println(F("Not connected"));
  }
  
  client.print(F("Players: "));
  client.println(numPlayers);
  
  client.print(F("Display State: "));
  switch (displayState) {
    case STATE_NULL: client.println(F("NULL")); break;
    case STATE_MAIN: client.println(F("MAIN")); break;
    case STATE_DETAIL: client.println(F("DETAIL")); break;
    case STATE_UTILS: client.println(F("UTILS")); break;
    case STATE_CLOCK: client.println(F("CLOCK")); break;
    default: client.println(F("UNKNOWN")); break;
  }
}

void telnetPrint(const String& message) {
  for (uint8_t i = 0; i < TELNET_MAX_CLIENTS; i++) {
    if (telnetClients[i] && telnetClients[i].connected()) {
      telnetClients[i].print(message);
    }
  }
}

void telnetPrintln(const String& message) {
  for (uint8_t i = 0; i < TELNET_MAX_CLIENTS; i++) {
    if (telnetClients[i] && telnetClients[i].connected()) {
      telnetClients[i].println(message);
    }
  }
}

// ************************* BUTTON ACTIONS ******************************

void buttonChange() {
  switch (displayState) {
    case STATE_MAIN:
      activeAction_Main++;
      // SKIP inactive players
      while (activeAction_Main >= numPlayers && activeAction_Main <= MAX_PLAYERS) activeAction_Main++;
      if (activeAction_Main >= MAX_PLAYERS) activeAction_Main = 0;
      break;
      
    case STATE_DETAIL:
      activeAction_Detail++;
      if (activeAction_Detail > 3) activeAction_Detail = 1;
      break;
      
    case STATE_UTILS:
      activeAction_Utils++;
      if (activeAction_Utils > 6) activeAction_Utils = 0;
      break;
      
    case STATE_CLOCK:
      // Accumulate time for current player before pausing/unpausing
      if (!clockPaused) {
        unsigned long now = millis();
        if (activeAction_Clock == 1 && p1_start_time > 0) {
          p1_time += (now - p1_start_time);
        } else if (activeAction_Clock == 2 && p2_start_time > 0) {
          p2_time += (now - p2_start_time);
        }
      }
      
      clockPaused = !clockPaused;
      
      if (!clockPaused) {
        // Resume: set start time for active player
        unsigned long now = millis();
        if (activeAction_Clock == 1) {
          p1_start_time = now;
          p2_start_time = 0;
        } else if (activeAction_Clock == 2) {
          p2_start_time = now;
          p1_start_time = 0;
        }
      } else {
        // Pause: clear start times
        p1_start_time = 0;
        p2_start_time = 0;
      }
      break;
      
    default:
      break;
  }
}

void buttonEnter() {
  switch (displayState) {
    case STATE_NULL:
      // Initialize default game from any state (including connection screen)
      playerName[0] = "Plr1";
      playerName[1] = "Plr2";
      playerName[2] = "Plr3";
      playerName[3] = "Plr4";
      numPlayers = 4;
      resetPlayerPoints(40);
      displayState = STATE_MAIN;
      Serial.println(F("Started default game via button"));
      break;
      
    case STATE_MAIN:
      if (activeAction_Main < MAX_PLAYERS) {
        displayState = STATE_DETAIL;
        activeAction_Detail = 1;
      } else if (activeAction_Main == MAX_PLAYERS) {
        displayState = STATE_UTILS;
        activeAction_Utils = 0;
      }
      break;
      
    case STATE_DETAIL:
      displayState = STATE_MAIN;
      break;
      
    case STATE_UTILS:
      switch (activeAction_Utils) {
        case 0: // Die Roll
          randomizeStart = true;
          activeAction_Main = 0;
          displayState = STATE_MAIN;
          break;
        case 1: // Reset 20
          resetPlayerPoints(20);
          activeAction_Main = 0;
          displayState = STATE_MAIN;
          break;
        case 2: // Reset 40
          resetPlayerPoints(40);
          activeAction_Main = 0;
          displayState = STATE_MAIN;
          break;
        case 3: // Save
          saveData();
          activeAction_Main = 0;
          displayState = STATE_MAIN;
          break;
        case 4: // Load
          loadData();
          activeAction_Main = 0;
          displayState = STATE_MAIN;
          break;
        case 5: // Clock
          activeAction_Clock = 1;
          displayState = STATE_CLOCK;
          break;
        case 6: // Back
          displayState = STATE_MAIN;
          break;
      }
      break;
      
    case STATE_CLOCK:
      activeAction_Main = 0;
      displayState = STATE_MAIN;
      break;
      
    default:
      break;
  }
}

void buttonPlus() {
  switch (displayState) {
    case STATE_MAIN:
      if (activeAction_Main < MAX_PLAYERS) {
        playerPoints[activeAction_Main][0] = 
          constrain(playerPoints[activeAction_Main][0] + 1, 0, 255);
      }
      break;
      
    case STATE_DETAIL:
      if (activeAction_Detail > 0) {
        uint8_t& damage = playerPoints[activeAction_Main][activeAction_Detail];
        uint8_t& total = playerPoints[activeAction_Main][0];
        
        damage = constrain(damage + 1, 0, 255);
        total = constrain(total - 1, 0, 255);
      }
      break;
      
    case STATE_CLOCK:
      // Accumulate time for current player before switching
      if (!clockPaused) {
        unsigned long now = millis();
        if (activeAction_Clock == 1 && p1_start_time > 0) {
          p1_time += (now - p1_start_time);
        } else if (activeAction_Clock == 2 && p2_start_time > 0) {
          p2_time += (now - p2_start_time);
        }
      }
      
      activeAction_Clock++;
      if (activeAction_Clock > 2) activeAction_Clock = 1;
      
      // Set new start time for the newly active player
      if (!clockPaused) {
        unsigned long now = millis();
        if (activeAction_Clock == 1) {
          p1_start_time = now;
          p2_start_time = 0;
        } else if (activeAction_Clock == 2) {
          p2_start_time = now;
          p1_start_time = 0;
        }
      } else {
        p1_start_time = 0;
        p2_start_time = 0;
      }
      break;
      
    default:
      break;
  }
}

void buttonMinus() {
  switch (displayState) {
    case STATE_MAIN:
      if (activeAction_Main < MAX_PLAYERS) {
        playerPoints[activeAction_Main][0] = 
          constrain(playerPoints[activeAction_Main][0] - 1, 0, 255);
      }
      break;
      
    case STATE_DETAIL:
      if (activeAction_Detail > 0) {
        uint8_t& damage = playerPoints[activeAction_Main][activeAction_Detail];
        uint8_t& total = playerPoints[activeAction_Main][0];
        
        if (damage > 0) {
          damage--;
          total = constrain(total + 1, 0, 255);
        }
      }
      break;
      
    case STATE_CLOCK:
      // Accumulate time for current player before switching
      if (!clockPaused) {
        unsigned long now = millis();
        if (activeAction_Clock == 1 && p1_start_time > 0) {
          p1_time += (now - p1_start_time);
        } else if (activeAction_Clock == 2 && p2_start_time > 0) {
          p2_time += (now - p2_start_time);
        }
      }
      
      activeAction_Clock++;
      if (activeAction_Clock > 2) activeAction_Clock = 1;
      
      // Set new start time for the newly active player
      if (!clockPaused) {
        unsigned long now = millis();
        if (activeAction_Clock == 1) {
          p1_start_time = now;
          p2_start_time = 0;
        } else if (activeAction_Clock == 2) {
          p2_start_time = now;
          p1_start_time = 0;
        }
      } else {
        p1_start_time = 0;
        p2_start_time = 0;
      }
      break;
      
    default:
      break;
  }
}

void buttonOption() {
  switch (displayState) {
    case STATE_MAIN:
      activeAction_Main = (activeAction_Main == MAX_PLAYERS) ? 0 : MAX_PLAYERS;
      break;
      
    case STATE_CLOCK:
      // Reset clock
      p1_start_time = 0;
      p1_time = 0;
      p2_start_time = 0;
      p2_time = 0;
      clockPaused = true;
      activeAction_Clock = 1;
      break;
      
    default:
      break;
  }
}

// *********************** OTA SETUP ****************************

void initializeOTA() {
    // Impostazioni base OTA
  ArduinoOTA.setHostname("MagicPoints");
  ArduinoOTA.setPassword("1234");

  // Callback per debug
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    clearDisplay();
    snprintf(line[0], LCD_COLS + 1, "**** OTA Update ****");
    snprintf(line[1], LCD_COLS + 1, " Update in progress ");
    snprintf(line[2], LCD_COLS + 1, "    Please wait...  ");
    snprintf(line[3], LCD_COLS + 1, "********************");
    printLines();
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd OTA Update");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    // Update progress on display
    // let's fill line[3] with ---> according to progress
    // calculate number of "-" to show (from 0 to 19)
    // last char is always ">"
    uint8_t progressChars = floor((progress * 19) / total);
    // Fill line[3] with progress bar: dashes, '>', then spaces
    uint8_t i = 0;
    for (; i < progressChars && i < LCD_COLS - 1; i++) {
      line[3][i] = '-';
    }
    if (i < LCD_COLS) {
      line[3][i++] = '>';
    }
    for (; i < LCD_COLS; i++) {
      line[3][i] = ' ';
    }
    line[3][LCD_COLS] = '\0';
    printLines();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Errore[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Authentication failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Address Error");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connection Error");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Error");
    else if (error == OTA_END_ERROR) Serial.println("End Error");
  });

  ArduinoOTA.begin();
  Serial.println("OTA ready!");
}

// ************************* SETUP ******************************

void setup() {
  initializeHardware();
  initializeDisplay();
  
  // Initialize game state
  resetPlayerPoints(0);
  
  initializeWiFi();
  setupWebServer();
  
  // Only setup Telnet server if WiFi is connected (not in AP config mode)
  if (!isConfigMode && WiFi.status() == WL_CONNECTED) {
    setupTelnetServer();
  }
  
  initializeOTA();

  initializeButtonDebounce();
  
  // Show final message
  printLines();
  const int cpufreq = ESP.getCpuFreqMHz();
  Serial.print(F("Setup complete! CPU Frequency: "));
  Serial.print(cpufreq);
  Serial.println(F(" MHz"));
  
  if (telnetConnected || !isConfigMode) {
    Serial.println(F("Telnet server available at port 23"));
  }
}

// ************************* MAIN LOOP ******************************

void loop() {
  static unsigned long lastDisplayUpdate = 0;
  static bool displayNeedsUpdate = true;
  unsigned long currentTime = millis();
  
  // Handle web server and OTA
  server.handleClient();
  yield();
  ArduinoOTA.handle();
  yield();
  
  // Handle Telnet clients (only if not in config mode)
  if (!isConfigMode && WiFi.status() == WL_CONNECTED) {
    handleTelnetClients();
    yield();
  }
  
  // Store previous state to detect changes
  static DisplayState previousState = STATE_NULL;
  static uint8_t previousActiveMain = 255;
  static uint8_t previousActiveDetail = 255;
  static uint8_t previousActiveUtils = 255;
  static uint8_t previousActiveClick = 255;
  
  // Button management handles its own timing
  buttonManagement();
  
  // Check if display needs updating
  if (displayState != previousState || 
      activeAction_Main != previousActiveMain ||
      activeAction_Detail != previousActiveDetail ||
      activeAction_Utils != previousActiveUtils ||
      activeAction_Clock != previousActiveClick ||
      displayState == STATE_CLOCK) { // Clock always needs updates
    
    displayNeedsUpdate = true;
    previousState = displayState;
    previousActiveMain = activeAction_Main;
    previousActiveDetail = activeAction_Detail;
    previousActiveUtils = activeAction_Utils;
    previousActiveClick = activeAction_Clock;
  }
  
  // Update display only when needed
  if (displayNeedsUpdate | (currentTime - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)) {
    lastDisplayUpdate = currentTime;
    updateDisplay();
    displayNeedsUpdate = false;
  }
  
  // Smaller delay to improve responsiveness
  delay(LOOP_DELAY);
}