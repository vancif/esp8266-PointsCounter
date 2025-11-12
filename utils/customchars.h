#ifndef CUSTOMCHARS_H
#define CUSTOMCHARS_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

// Custom Characters Definitions
#define CHAR_DEGREE 0
#define CHAR_LEFT_ARROW 1
#define CHAR_RIGHT_ARROW 2
#define CHAR_HOURGLASS 3
#define CHAR_A_ACCENT 4  // à
#define CHAR_E_ACCENT 5  // è

// Custom character bitmap data
extern const byte customChars[6][8] PROGMEM;

// Function to initialize custom characters on LCD
void initializeCustomCharacters(LiquidCrystal_I2C& lcd);

#endif // CUSTOMCHARS_H