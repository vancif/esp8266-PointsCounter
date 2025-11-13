// Implementazione dei caratteri personalizzati per ESP8266 Points Counter
// Questo file è incluso da customchars.h per aggirare le limitazioni dell'IDE Arduino

// Custom character bitmap definitions stored in program memory
const byte customChars[6][8] PROGMEM = {
  // Degree symbol
  {0b11100, 0b10100, 0b11100, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000},
  // Left arrow
  {0b00000, 0b00100, 0b01000, 0b11111, 0b11111, 0b01000, 0b00100, 0b00000},
  // Right arrow
  {0b00000, 0b00100, 0b00010, 0b11111, 0b11111, 0b00010, 0b00100, 0b00000},
  // Hourglass
  {0b00000, 0b11111, 0b01110, 0b00100, 0b01110, 0b11111, 0b00000, 0b00000},
  // à
  {0b00010, 0b00001, 0b01110, 0b00001, 0b01111, 0b10001, 0b01111, 0b00000},
  // è
  {0b00010, 0b00001, 0b01110, 0b10001, 0b11111, 0b10000, 0b01110, 0b00000}
};

// Function to initialize custom characters on LCD
void initializeCustomCharacters(LiquidCrystal_I2C& lcd) {
  // Load custom characters
  for (uint8_t i = 0; i < 6; i++) {
    byte charData[8];
    memcpy_P(charData, customChars[i], 8);
    lcd.createChar(i, charData);
  }
}