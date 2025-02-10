# esp8266-PointsCounter: ESP8266 LCD Scoreboard with Web Interface
ESP8266-based score tracker with an I2C LCD and web interface for managing player points. 🚀

## Overview

**esp8266-PointsCounter** is an ESP8266-based scoreboard system that uses an I2C LCD display and a web interface to manage player points. The system allows interaction via physical buttons and a web interface, making it suitable for tabletop games, competitions, and similar applications.

## Features

- **LCD Display (I2C, 20x4)**: Shows player names and scores.
- **ESP8266 Web Server**: Allows updating scores and managing players via a web interface.
- **Physical Buttons**: Controls for incrementing, decrementing, and navigating scores.
- **Wi-Fi Connectivity**: Connects to a predefined Wi-Fi network or creates an access point if no network is available.
- **mDNS Support**: Access the web interface via `http://MagicPoints.local`.

## Hardware Requirements

- ESP8266 (e.g., NodeMCU or Wemos D1 Mini)
- 20x4 I2C LCD display
- Push buttons (connected to GPIOs)
- Resistors for pulldown

## Wiring Diagram

| Component       | ESP8266 Pin  |
|---------------|-------------|
| LCD SDA       | D2          |
| LCD SCL       | D1          |
| Button 1 (+)  | D5          |
| Button 2 (-)  | D6          |
| Button 3 (CH) | D8          |

## Installation

1. **Install Dependencies**: 
   - Add the following libraries in the Arduino IDE:
     - `LiquidCrystal_I2C`
     - `ESP8266WiFi`
     - `ESP8266WebServer`
     - `ESP8266mDNS`

2. **Upload the Code**:
   - Open the `.ino` file in the Arduino IDE.
   - Set the correct board (ESP8266).
   - Flash the firmware to your ESP8266.

3. **Configure Wi-Fi**:
   - Edit the `setup()` function and add your Wi-Fi credentials in:
     ```cpp
     wifiMulti.addAP("Your_SSID", "Your_PASSWORD");
     ```

4. **Run the System**:
   - Power the ESP8266.
   - Check the LCD screen for the assigned IP address.
   - Access the web interface using the displayed IP or `http://MagicPoints.local` (if mDNS is supported).

## Web Interface

- Input player names and initial scores.
- Adjust scores using the provided buttons.
- Navigate between players and utility functions.

## Physical Button Functions

| Button | Function |
|--------|----------|
| **+ (D5)** | Increase selected score |
| **- (D6)** | Decrease selected score |
| **CH (D8)** | Cycle through players/options |
| **ENT (Long Press D8)** | Select an option |
| **OPT (Press both + and -** | Access Utils menu |

## License

This project is open-source under the MIT License.

---

Enjoy your game with esp8266-PointsCounter! 🎲📟
