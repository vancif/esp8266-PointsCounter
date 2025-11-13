#include "display.h"
#include "../utils/customchars.h"

// Constants from main file
#define LCD_COLS 20
#define LCD_ROWS 4
#define MAX_PLAYERS 4

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