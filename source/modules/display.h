#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "../constants.h"

// Forward declarations for global variables
extern LiquidCrystal_I2C lcd;
extern char line[LCD_ROWS][LCD_COLS + 1];  // LCD_ROWS x (LCD_COLS + 1) - more explicit sizing
extern String playerName[];
extern uint8_t playerPoints[MAX_PLAYERS][MAX_PLAYERS];
extern uint8_t numPlayers;
extern uint8_t activeAction_Main;
extern uint8_t activeAction_Detail;
extern uint8_t activeAction_Utils;
extern uint8_t activeAction_Clock;
extern bool randomizeStart;
extern bool clockPaused;
extern unsigned long p1_time;
extern unsigned long p2_time;
extern unsigned long p1_start_time;
extern unsigned long p2_start_time;

// Display state enum
enum DisplayState {
  STATE_NULL,
  STATE_MAIN,
  STATE_DETAIL,
  STATE_UTILS,
  STATE_CLOCK
};

extern DisplayState displayState;

// Display management functions
void printLines();
void updateDisplay();
void updateMainDisplay();
void updateDetailDisplay();
void updateUtilsDisplay();
void updateClockDisplay();
void clearDisplay();

// Display initialization
void initializeDisplay();

#include "display_impl.h"

#endif // DISPLAY_H