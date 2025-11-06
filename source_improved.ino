#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// ************************* CONSTANTS AND CONFIGURATION ******************************

// System Configuration
#define LOOP_DELAY 25  // milliseconds

// WiFi Configuration
#define AP_NAME "Fallback_AP_SSID"
#define AP_PWD "Fallback_AP_PWD"
#define SSID_AP_1 "SSID_AP_1"
#define PWD_AP_1 "PWD_AP_1"

// Button Configuration
#define LONG_PRESS_TIME 350
#define BUTTON_DELAY 150

// Pin Definitions
#define BTN_MINUS D5
#define BTN_PLUS D6
#define BTN_MAIN D8

// Display Configuration
#define LCD_ADDRESS 0x27
#define LCD_COLS 20
#define LCD_ROWS 4

// Game Configuration
#define MAX_PLAYERS 4
#define NAME_LENGTH 6
#define EEPROM_SIZE 512

// LED Control
#define LED_ON digitalWrite(LED_BUILTIN, LOW)
#define LED_OFF digitalWrite(LED_BUILTIN, HIGH)

// Custom Characters
#define CHAR_DEGREE 0
#define CHAR_LEFT_ARROW 1
#define CHAR_RIGHT_ARROW 2
#define CHAR_HOURGLASS 3

// ************************* ENUMS ******************************

enum DisplayState {
  STATE_NULL,
  STATE_MAIN,
  STATE_DETAIL,
  STATE_UTILS,
  STATE_CLOCK
};

// ******************* GLOBAL VARIABLES ****************************

// Network components
ESP8266WiFiMulti wifiMulti;
ESP8266WebServer server(80);
IPAddress myIP;

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

// Clock state
unsigned long p1_start_time = 0;
unsigned long p1_time = 0;
unsigned long p2_start_time = 0;
unsigned long p2_time = 0;
bool clockPaused = true;

// ************************* CUSTOM CHARACTERS ******************************

const byte customChars[4][8] PROGMEM = {
  // Degree symbol
  {0b11100, 0b10100, 0b11100, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000},
  // Left arrow
  {0b00000, 0b00100, 0b01000, 0b11111, 0b11111, 0b01000, 0b00100, 0b00000},
  // Right arrow
  {0b00000, 0b00100, 0b00010, 0b11111, 0b11111, 0b00010, 0b00100, 0b00000},
  // Hourglass
  {0b00000, 0b11111, 0b01110, 0b00100, 0b01110, 0b11111, 0b00000, 0b00000}
};

// ************************* HTML STORED IN PROGMEM ******************************

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<html>
<head><meta name="viewport" content="width=device-width, initial-scale=1.0" /></head>
<body>
<input maxlength="6" size="10" type="text" id="n0"><input maxlength="2" size="3" type="text" id="p0"><br><br>
<input maxlength="6" size="10" type="text" id="n1"><input maxlength="2" size="3" type="text" id="p1"><br><br>
<input maxlength="6" size="10" type="text" id="n2"><input maxlength="2" size="3" type="text" id="p2"><br><br>
<input maxlength="6" size="10" type="text" id="n3"><input maxlength="2" size="3" type="text" id="p3"><br><br>
<button onclick="send()">Update</button><br><br>
<button onclick="operate('1')" style="width:75px;height:75px;font:15pt Arial;">+</button>
<button onclick="operate('2')" style="width:75px;height:75px;font:15pt Arial;">-</button><br><br>
<button onclick="operate('3')" style="width:75px;height:75px;font:15pt Arial;">CH</button>
<button onclick="operate('4')" style="width:75px;height:75px;font:15pt Arial;">ENT</button>
<button onclick="operate('5')" style="width:75px;height:75px;font:15pt Arial;">OPT</button>
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

// ************************* FUNCTION PROTOTYPES ******************************

// Setup functions
void initializeHardware();
void initializeDisplay();
void initializeWiFi();
void setupWebServer();

// Button handling
void buttonManagement();

// Display management
void printLines();
void updateDisplay();
void updateMainDisplay();
void updateDetailDisplay();
void updateUtilsDisplay();
void updateClockDisplay();

// Data management
void saveData();
void loadData();
bool validateEEPROMData();

// Web handlers
void handleRoot();
void handleAction();
void handleNotFound();

// Button actions
void buttonChange();
void buttonEnter();
void buttonPlus();
void buttonMinus();
void buttonOption();

// Utility functions
template<typename T> const T& getArrayElement(const T* array, int pos, int length);
void resetPlayerPoints(uint8_t startingPoints);

// ************************* UTILITY FUNCTIONS ******************************

void printLines() {
  for (uint8_t i = 0; i < LCD_ROWS; i++) {
    lcd.setCursor(0, i);
    lcd.print(line[i]);
  }
}

template<typename T> 
const T& getArrayElement(const T* array, int pos, int length) {
  return array[pos % length];
}

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
  
  // Load custom characters
  for (uint8_t i = 0; i < 4; i++) {
    byte charData[8];
    memcpy_P(charData, customChars[i], 8);
    lcd.createChar(i, charData);
  }

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

  // Clear display and show WiFi setup message
  for (uint8_t i = 0; i < LCD_ROWS; i++) {
    snprintf(line[i], LCD_COLS + 1, "%*s", LCD_COLS, "");
  }
  snprintf(line[3], LCD_COLS + 1, "WiFi setup...");
  printLines();
  
  // Add Wi-Fi networks
  wifiMulti.addAP(SSID_AP_1, PWD_AP_1);
  // Add more networks as needed:
  // wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
  
  unsigned long wifiStartTime = millis();
  const unsigned long WIFI_TIMEOUT = 10000; // 10 seconds
  bool wifiConnected = false;
  
  // Attempt to connect to WiFi
  while (millis() - wifiStartTime < WIFI_TIMEOUT && !wifiConnected) {
    if (wifiMulti.run() == WL_CONNECTED) {
      wifiConnected = true;
      break;
    }
    delay(250);
    Serial.print('.');
  }
  
  if (wifiConnected) {
    // Successfully connected to WiFi
    myIP = WiFi.localIP();
    Serial.println();
    Serial.print(F("Connected to: "));
    Serial.println(WiFi.SSID());
    Serial.print(F("IP address: "));
    Serial.println(myIP);
    
    // Clear display and show temporary mDNS setup message
    for (uint8_t i = 0; i < LCD_ROWS; i++) {
      snprintf(line[i], LCD_COLS + 1, "%*s", LCD_COLS, ""); // Fill with spaces
    }
    printLines();
    snprintf(line[2], LCD_COLS + 1, "%.19s", myIP.toString().c_str());
    snprintf(line[3], LCD_COLS + 1, "mDNS setup...");
    printLines();
    
    // Start mDNS responder
    if (MDNS.begin("MagicPoints")) {
      Serial.println(F("mDNS responder started"));
    } else {
      Serial.println(F("Error setting up MDNS responder!"));
    }
    
    // Clear and prepare final WiFi display messages
    for (uint8_t i = 0; i < LCD_ROWS; i++) {
      snprintf(line[i], LCD_COLS + 1, "%*s", LCD_COLS, ""); // Fill with spaces
    }
    printLines();
    snprintf(line[0], LCD_COLS + 1, "Connect to this wifi");
    snprintf(line[1], LCD_COLS + 1, "to start a game");
    snprintf(line[2], LCD_COLS + 1, "SSID: %.13s", WiFi.SSID().c_str());
    snprintf(line[3], LCD_COLS + 1, "IP: %.15s", myIP.toString().c_str());
    
  } else {
    // WiFi connection failed, start Access Point
    Serial.println();
    Serial.println(F("WiFi not available, starting AP mode"));
    
    // Clear all display lines first
    for (uint8_t i = 0; i < LCD_ROWS; i++) {
      snprintf(line[i], LCD_COLS + 1, "%*s", LCD_COLS, ""); // Fill with spaces
    }
    
    // Show temporary status
    snprintf(line[2], LCD_COLS + 1, "WiFi not available");
    snprintf(line[3], LCD_COLS + 1, "Starting AP...");
    printLines();
    delay(1000);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_NAME, AP_PWD, 1, 0);
    myIP = WiFi.softAPIP();
    
    Serial.print(F("AP started: "));
    Serial.println(AP_NAME);
    Serial.print(F("AP IP: "));
    Serial.println(myIP);
    
    // Clear and prepare final AP display messages
    for (uint8_t i = 0; i < LCD_ROWS; i++) {
      snprintf(line[i], LCD_COLS + 1, "%*s", LCD_COLS, ""); // Fill with spaces
    }
    printLines();
    snprintf(line[0], LCD_COLS + 1, "Connect to this wifi");
    snprintf(line[1], LCD_COLS + 1, "%.19s", AP_NAME);        // Limit to 19 chars + null terminator
    snprintf(line[2], LCD_COLS + 1, "%.19s", AP_PWD);         // Limit to 19 chars + null terminator  
    snprintf(line[3], LCD_COLS + 1, "%.19s", myIP.toString().c_str()); // Limit IP display
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_POST, handleAction);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println(F("HTTP server started"));
}

// ************************* BUTTON HANDLING ******************************

void buttonManagement() {
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
    } else if ( pressDuration >= LONG_PRESS_TIME ) {
      // LONG PRESS
      buttonEnter();
    }
  }

  // save the last state
  lastButtonState = currentState;

  //************* OTHERS SIMPLE BUTTONS *********
  if (digitalRead(BTN_MINUS) == HIGH && digitalRead(BTN_PLUS) == LOW) buttonMinus();
  if (digitalRead(BTN_PLUS) == HIGH && digitalRead(BTN_MINUS) == LOW) buttonPlus();
  if (digitalRead(BTN_PLUS) == HIGH && digitalRead(BTN_MINUS) == HIGH) buttonOption();
}

// ************************* DISPLAY MANAGEMENT ******************************

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
}

void updateMainDisplay() {
  // Clear all lines first
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
  
  // Update running timers
  if (!clockPaused) {
    switch (activeAction_Clock) {
      case 1:
        if (p1_start_time == 0) {
          p1_start_time = now - BUTTON_DELAY - LOOP_DELAY;
        } else {
          p1_time += (now - p1_start_time);
          p1_start_time = now;
        }
        break;
      case 2:
        if (p2_start_time == 0) {
          p2_start_time = now - BUTTON_DELAY - LOOP_DELAY;
        } else {
          p2_time += (now - p2_start_time);
          p2_start_time = now;
        }
        break;
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
  server.send_P(200, "text/html", HTML_PAGE);
  LED_OFF;
}

void handleAction() {
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
  
  server.send(200, "application/json", "{\"status\":\"success\"}");
  LED_OFF;
}

void handleNotFound() {
  LED_ON;
  server.send(404, "text/plain", "404: Not found");
  LED_OFF;
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
      clockPaused = !clockPaused;
      if (!clockPaused) {
        unsigned long now = millis();
        if (activeAction_Clock == 1) {
          p1_start_time = now;
        } else if (activeAction_Clock == 2) {
          p2_start_time = now;
        }
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
      activeAction_Clock++;
      if (activeAction_Clock > 2) activeAction_Clock = 1;
      
      // Reset timer states when switching players
      if (activeAction_Clock == 1) {
        p2_start_time = 0;
      } else if (activeAction_Clock == 2) {
        p1_start_time = 0;
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
      activeAction_Clock++;
      if (activeAction_Clock > 2) activeAction_Clock = 1;
      
      // Reset timer states when switching players
      if (activeAction_Clock == 1) {
        p2_start_time = 0;
      } else if (activeAction_Clock == 2) {
        p1_start_time = 0;
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

// ************************* SETUP ******************************

void setup() {
  initializeHardware();
  initializeDisplay();
  
  // Initialize game state
  resetPlayerPoints(0);
  
  initializeWiFi();
  setupWebServer();
  
  // Show final message
  printLines();
}

// ************************* MAIN LOOP ******************************

void loop() {
  // Handle web server requests
  server.handleClient();

  buttonManagement();

  // Update display
  updateDisplay();
  printLines();

  // Small delay to prevent excessive CPU usage and allow WiFi stack processing
  delay(LOOP_DELAY);
}