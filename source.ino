#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>


// ******************* GLOBAL VARIABLES AND STATICS ****************************

ESP8266WiFiMulti wifiMulti;  // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

IPAddress myIP;

ESP8266WebServer server(80);  // Create a webserver object that listens for HTTP request on port 80

void handleRoot();  // function prototypes for HTTP handlers
void handleNotFound();
void handleToggle();

LiquidCrystal_I2C lcd(0x27, 20, 4);  // Set the LCD address to 0x27 for a 20 chars and 4 line display

// custom char °
static byte grade[8] = {
  0b11100,
  0b10100,
  0b11100,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

static byte leftarrow[8] = {
  0b00000,
  0b00100,
  0b01000,
  0b11111,
  0b11111,
  0b01000,
  0b00100,
  0b00000
};

static byte rightarrow[8] = {
  0b00000,
  0b00100,
  0b00010,
  0b11111,
  0b11111,
  0b00010,
  0b00100,
  0b00000
};

static byte hourglass[8] = {
  0b00000,
  0b11111,
  0b01110,
  0b00100,
  0b01110,
  0b11111,
  0b00000,
  0b00000
};

#define LED_ON digitalWrite(LED_BUILTIN, LOW);
#define LED_OFF digitalWrite(LED_BUILTIN, HIGH);

#define AP_NAME "fallback_AP_name"
#define AP_PWD "fallback_AP_pwd"

#define EEPROM_SIZE 512  // Dimensione massima della EEPROM
#define NAME_LENGTH 6    // Lunghezza massima del nome del giocatore

char line[4][21];

String playerName[4];

uint8_t playerPoints[4][4];

uint8_t numPlayers = 0;

uint8_t activeAction_Main = 0;
uint8_t activeAction_Detail = 0;
uint8_t activeAction_Utils = 0;

String displayState = "null";

#define LONG_PRESS_TIME  500
#define BUTTON_DELAY 150

// Variables will change:
int lastState = LOW;  // the previous state from the input pin
int currentState;     // the current reading from the input pin
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

bool randomizeStart = false;


// ************************ utilities *******************************
void printLines() {
  // Print one line at a time
  for (uint8_t i = 0; i <= 3; i++) {
    lcd.setCursor(0, i);
    lcd.print(line[i]);
  }
}

template<typename T> const T getArrayElement(T* x,int pos = 0, int length = 0){
  if (pos >= length) {
    return x[pos-length];
  } else return x[pos];
}


void saveData() {
  uint8_t addr = 0;  // Indirizzo iniziale EEPROM

  char nameBuffer[6];

  // Salvataggio dei nomi
  for (uint8_t i = 0; i < 4; i++) {
    playerName[i].toCharArray(nameBuffer, NAME_LENGTH);
    EEPROM.put(addr, nameBuffer);  // Scrive il nome come array di caratteri
    addr += NAME_LENGTH;   // Sposta l'indirizzo per il prossimo nome
  }

  // Salvataggio dei punteggi
  for (uint8_t i = 0; i < 4; i++) {
    for (uint8_t j = 0; j < 4; j++) {
      EEPROM.put(addr, playerPoints[i][j]);  // Scrive il punteggio
      addr += sizeof(uint8_t);   // Sposta l'indirizzo di 1 byte
    }
  }

  // scrittura numero giocatori
  EEPROM.put(addr,numPlayers);

  EEPROM.commit();  // Salva i dati nella memoria flash
  Serial.println("Dati salvati nella EEPROM!");
}

void loadData() {
  int addr = 0;  // Indirizzo iniziale EEPROM

  char nameBuffer[6];

  // Lettura dei nomi
  for (uint8_t i = 0; i < 4; i++) {
    EEPROM.get(addr, nameBuffer);  // Legge il nome
    playerName[i] = String(nameBuffer);
    addr += NAME_LENGTH;
  }
  

  // Lettura dei punteggi
  for (uint8_t i = 0; i < 4; i++) {
    for (uint8_t j = 0; j < 4; j++) {
      EEPROM.get(addr, playerPoints[i][j]);  // Legge il punteggio
      addr += sizeof(uint8_t);
    }
  }

  // lettura numero giocatori
  EEPROM.get(addr,numPlayers);

  Serial.println("Dati caricati dalla EEPROM!");
}


// ******************************* SETUP ***************************************

void setup() {
  //init LED
  pinMode(LED_BUILTIN, OUTPUT);
  LED_OFF

  // Inizializza EEPROM
  EEPROM.begin(EEPROM_SIZE);

  //init display
  lcd.begin(D2, D1);
  lcd.backlight();
  lcd.createChar(0, grade);
  lcd.createChar(1, leftarrow);
  lcd.createChar(2, rightarrow);
  lcd.createChar(3, hourglass);

  //show boot message
  lcd.setCursor(0, 0);
  lcd.print("ESP8266");
  lcd.setCursor(0, 1);
  lcd.print("Hi, booting up...");

  // init all points to 0
  for (uint8_t i = 0; i<4; i++){
    for (uint8_t j = 0; j<4; j++){
      playerPoints[i][j] = 0;
    }
  }

  //init buttons
  pinMode(D5, INPUT);
  pinMode(D6, INPUT);
  pinMode(D8, INPUT);

  //serial setup
  lcd.setCursor(0, 3);
  lcd.print("Serial setup        ");
  Serial.begin(9600);
  delay(10);
  Serial.println('\n');

  //wifi setup
  lcd.setCursor(0, 3);
  lcd.print("WiFi setup          ");
  Serial.println("Wifi Connecting ...");

  // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("ssid_from_AP_1", "your_password_for_AP_1");
  //wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
  //wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

  // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above

  unsigned long wifiConfigTS = millis();
  bool WifiNA = false;
  while (wifiMulti.run(10000) != WL_CONNECTED && !WifiNA) {
    delay(250);
    Serial.print('.');
    if (millis() > wifiConfigTS + 10000) WifiNA = true;
  }
  if (!WifiNA) { // WIFI found!
    myIP = WiFi.localIP();
    Serial.println('\n');
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());  // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.println(myIP);  // Send the IP address of the ESP8266 to the computer

    lcd.setCursor(0, 2);
    lcd.print(myIP);
    lcd.setCursor(0, 3);
    lcd.print("mDNS setup.         ");

    if (MDNS.begin("MagicPoints")) {  // Start the mDNS responder for MagicPoints.local
      Serial.println("mDNS responder started");
    } else {
      Serial.println("Error setting up MDNS responder!");
    }
    sprintf(line[0],"Connect to this wifi");
    sprintf(line[1],"to start a game     ");
    sprintf(line[2],"ESSID: %13s",WiFi.SSID());
    sprintf(line[3],"IP: %16s",myIP.toString().c_str());
  } else {    // WIFI not found!
    Serial.println('\n');
    Serial.print("Wifi Not Available");
    lcd.setCursor(0, 2);
    lcd.print("Wifi not available");
    delay(1000);
    Serial.print("Configuring access point...");
    lcd.setCursor(0, 3);
    lcd.print("Configuring AP...   ");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_NAME,AP_PWD,1,0);
    myIP = WiFi.softAPIP();
    sprintf(line[0],"Connect to this wifi");
    sprintf(line[1],AP_NAME);
    sprintf(line[2],AP_PWD);
    sprintf(line[3],"%20s",myIP.toString().c_str());
  }

  delay(500);

  // webserver config
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_POST, handleAction);
  server.onNotFound(handleNotFound);

  // webserver start
  server.begin();
  Serial.println("HTTP server started");

  // show start screen on lcd
  printLines();
}


// ******************************** MAIN LOOP *******************************


void loop() {

  // webserver handle
  server.handleClient();


  // *************************************************
  // ***************** BUTTON MANAGEMENT *************
  // *************************************************
  // ********* D8 LONG-SHORT PRESS *********
  int currentState = digitalRead(D8);

  if (lastState == LOW && currentState == HIGH)       // button is pressed
    pressedTime = millis();
  else if (lastState == HIGH && currentState == LOW) { // button is released
    releasedTime = millis();

    long pressDuration = releasedTime - pressedTime;

    if ( pressDuration < LONG_PRESS_TIME ) {
      // SHORT PRESS
      tastoCH();
    } else if ( pressDuration >= LONG_PRESS_TIME ) {
      // LONG PRESS
      tastoENT();
    }

  } 

  // save the the last state
  lastState = currentState;

  //************* OTHERS SIMPLE BUTTONS *********
  if (digitalRead(D5) == HIGH && digitalRead(D6) == LOW) tastoMENO();
  if (digitalRead(D6) == HIGH && digitalRead(D5) == LOW) tastoPIU();
  if (digitalRead(D6) == HIGH && digitalRead(D5) == HIGH) tastoOPT();


  // **************************************************
  // ***************** DISPLAY MANAGEMENT *************
  // **************************************************
  // ******************* MAIN DISPLAY *****************
  if (displayState == "main") {
    for (uint8_t i = 0; i <= 3; i++){
      if (i < numPlayers) sprintf(line[i]," %6s: %2i         ",playerName[i],playerPoints[i][0]);
      else sprintf(line[i],"                    ");
      line[i][20] = '\0';
    }
    strncpy(&line[3][14], "Utils",5);

    if (randomizeStart && random(0,11)<=9){
      activeAction_Main++;
      if (activeAction_Main>numPlayers-1) activeAction_Main = 0;
      line[0][19] = 3;
      delay(250);
    } else {
      randomizeStart = false;
    }

    if (activeAction_Main <= 3) {
      line[activeAction_Main][0] = 2;
    } else if (activeAction_Main == 4){
      line[3][19] = 1;
    }

  // ****************** DETAIL DISPLAY ****************
  } else if (displayState == "detail") {
    sprintf(line[0],"Comm.Dmg %6s P:%2i",playerName[activeAction_Main],playerPoints[activeAction_Main][0]);

    sprintf(line[1],"%6s: %2i          ",getArrayElement(playerName,activeAction_Main+1,4),playerPoints[activeAction_Main][1]);

    sprintf(line[2],"%6s: %2i          ",getArrayElement(playerName,activeAction_Main+2,4),playerPoints[activeAction_Main][2]);

    sprintf(line[3],"%6s: %2i    BACK  ",getArrayElement(playerName,activeAction_Main+3,4),playerPoints[activeAction_Main][3]);

      switch (activeAction_Detail){
        case 0: line[3][18] = 1;
        break;
        case 1: line[1][11] = 1;
        break;
        case 2: line[2][11] = 1;
        break;
        case 3: line[3][11] = 1;
        break;
      }

  // *************** UTILS DISPLAY *******************
  } else if (displayState == "utils"){
    sprintf(line[0]," Die Roll      SAVE ");
    sprintf(line[1]," Reset 20      LOAD ");
    sprintf(line[2]," Reset 40           ");
    sprintf(line[3],"        BACK        ");
      switch (activeAction_Utils){
        case 0: line[0][0] = 2;
        break;
        case 1: line[1][0] = 2;
        break;
        case 2: line[2][0] = 2;
        break;
        case 3: line[0][19] = 1;
        break;
        case 4: line[1][19] = 1;
        break;
        case 5: line[3][12] = 1;
        break;
      }
  }
  
  printLines();

}


// ************************ WEBSERVER HANDLES ******************************

void handleRoot() {
  LED_ON
  server.send(200, "text/html", "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" /></head><body>\
  \
  <input maxlength=6 size=10 type=\"text\" id=n0 ><input maxlength=2 size=3 type=\"text\" id=p0 ><br /><br />\
  \
  <input maxlength=6 size=10 type=\"text\" id=n1 ><input maxlength=2 size=3 type=\"text\" id=p1 ><br /><br />\
  \
  <input maxlength=6 size=10 type=\"text\" id=n2 ><input maxlength=2 size=3 type=\"text\" id=p2 ><br /><br />\
  \
  <input maxlength=6 size=10 type=\"text\" id=n3 ><input maxlength=2 size=3 type=\"text\" id=p3 ><br /><br />\
  \
  <button onclick=\"send()\">Update</button>\
  <br>\
  <br>\
  <button id=bp onclick=\"operate('1')\" style=\"width:75px;height:75px;font:15pt Arial;\">+</button>\
  <button id=bm onclick=\"operate('2')\"style=\"width:75px;height:75px;font:15pt Arial;\">-</button>\
  <br>\
  <br>\
  <button id=bc onclick=\"operate('3')\"style=\"width:75px;height:75px;font:15pt Arial;\">CH</button>\
  <button id=be onclick=\"operate('4')\"style=\"width:75px;height:75px;font:15pt Arial;\">ENT</button>\
  <button id=bo onclick=\"operate('5')\"style=\"width:75px;height:75px;font:15pt Arial;\">OPT</button>\
  <script>\
  function operate(act){\
    window.navigator.vibrate(100);\
    var data = new FormData();\
    data.append(\"action\",act);\
    data.append(\"create\",\"false\");\
    fetch(\"/update\",\
      {\
        method: \"POST\",\
        body: data\
      });\
  };\
  function send(){\
    window.navigator.vibrate(100);\
    var data = new FormData();\
    data.append(\"create\",\"true\");\
    data.append(\"n0\",document.getElementById(\"n0\").value);\
    data.append(\"n1\",document.getElementById(\"n1\").value);\
    data.append(\"n2\",document.getElementById(\"n2\").value);\
    data.append(\"n3\",document.getElementById(\"n3\").value);\
    data.append(\"p0\",document.getElementById(\"p0\").value);\
    data.append(\"p1\",document.getElementById(\"p1\").value);\
    data.append(\"p2\",document.getElementById(\"p2\").value);\
    data.append(\"p3\",document.getElementById(\"p3\").value);\
    fetch(\"/update\",\
      {\
        method: \"POST\",\
        body: data\
      });\
  };\
  </script></body>");
  LED_OFF
}

void handleAction() {
  LED_ON

  if (server.arg("create") == "true") {
    displayState = "main";
    numPlayers = 0;
    for (uint8_t i = 0; i<4; i++){
      String argN = "n" + String(i);
      String argP = "p" + String(i);
      Serial.print("player name: ");
      Serial.println(server.arg(argN));
      if (server.arg(argN) != "") numPlayers++;
      playerName[i] = server.arg(argN);
      playerPoints[i][0] = server.arg(argP).toInt();
      Serial.print("num players:");
      Serial.println(numPlayers);
    }
  } else if (server.arg("action")) {
    switch (server.arg("action").toInt()) {
      case 1: tastoPIU(); break;
      case 2: tastoMENO(); break;
      case 3: tastoCH(); break;
      case 4: tastoENT(); break;
      case 5: tastoOPT(); break;
    }
  }

  server.send(200, "application/json", "{\"status\":\"success\"}");

  LED_OFF
}


void handleNotFound() {
  LED_ON
  server.send(404, "text/plain", "404: Not found");  // Send HTTP status 40 (Not Found) when there's no handler for the URI in the request
  LED_OFF
}

//*************************** TASTO CH ***************************
void tastoCH(){
  if (displayState == "main"){

    activeAction_Main++;
    
    while (activeAction_Main>numPlayers-1 && activeAction_Main < 4) activeAction_Main++;

    if (activeAction_Main>3) activeAction_Main = 0;

  } else if (displayState == "detail"){

    activeAction_Detail++;
    if (activeAction_Detail > 3) activeAction_Detail = 0;

  } else if (displayState == "utils"){

    activeAction_Utils++;
    if (activeAction_Utils > 5) activeAction_Utils = 0;

  }
  delay(BUTTON_DELAY);
}

//*************************** TASTO ENT **************************
void tastoENT(){
  if (displayState == "null"){
    playerName[0] = "Plr1";
    playerName[1] = "Plr2";
    playerName[2] = "Plr3";
    playerName[3] = "Plr4";
    numPlayers = 4;
    for (uint8_t i = 0; i<4; i++){
      playerPoints[i][0] = 40;
      for (uint8_t j = 1; j<4; j++){
        playerPoints[i][j] = 0;
      }
    }
    displayState = "main";
  }
  else if (displayState == "main") {
    if (activeAction_Main <= 3) {
      displayState = "detail";
      activeAction_Detail = 0;
    } else if (activeAction_Main == 4){
      displayState = "utils";
      activeAction_Utils = 0;
    }
  } else if (displayState == "detail"){
    displayState = "main";
  } else if (displayState == "utils"){
    switch (activeAction_Utils) {
      case 0:
      randomizeStart = true;
      break;
      case 1:
      for (uint8_t i = 0; i<4; i++){
        playerPoints[i][0] = 20;
        for (uint8_t j = 1; j<4; j++){
          playerPoints[i][j] = 0;
        }
      }
      activeAction_Main = 0;
      break;
      case 2:
      for (uint8_t i = 0; i<4; i++){
        playerPoints[i][0] = 40;
        for (uint8_t j = 1; j<4; j++){
          playerPoints[i][j] = 0;
        }
      }
      activeAction_Main = 0;
      break;
      case 3: saveData(); activeAction_Main = 0;
      break;
      case 4: loadData(); activeAction_Main = 0;
      break;
      case 5:
      break;
    }
    displayState = "main";
  }
  // delay not really necessary as this button work "onButtonRelease"
  // delay(BUTTON_DELAY);
}

//*************************** TASTO + ***************************
void tastoPIU(){
  if (displayState == "main" && activeAction_Main <= 3) playerPoints[activeAction_Main][0]++;
  else if (displayState == "detail" && activeAction_Detail > 0) {
    playerPoints[activeAction_Main][activeAction_Detail]++;
    playerPoints[activeAction_Main][0]--;
  }
  delay(BUTTON_DELAY);
}

//*************************** TASTO - ***************************
void tastoMENO(){
  if (displayState == "main" && activeAction_Main <= 3) playerPoints[activeAction_Main][0]--;
  else if (displayState == "detail" && activeAction_Detail > 0) {
    playerPoints[activeAction_Main][activeAction_Detail]--;
    playerPoints[activeAction_Main][0]++;
  }
  delay(BUTTON_DELAY);
}

//*************************** TASTO OPT *************************
void tastoOPT(){
  if (displayState == "main") {
    if (activeAction_Main != 4) activeAction_Main = 4;
    else activeAction_Main = 0;
  }
    
}
