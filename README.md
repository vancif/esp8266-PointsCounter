# ESP8266 Points Counter
A comprehensive scoreboard system for tabletop games, featuring both physical buttons and web interface control.

## What This Project Does

This ESP8266-based device creates a **digital scoreboard** perfect for tabletop games like Magic: The Gathering, board games, or any scenario where you need to track multiple players' scores. The system displays player information on a 20x4 LCD screen and provides both **physical button controls** and a **web interface** for managing the game.

### Key Features

- **Dual Interface**: Control via physical buttons or web browser
- **Multi-Player Support**: Track up to 4 players simultaneously  
- **Advanced Scoring**: Main points + detailed damage tracking per opponent
- **Data Persistence**: Save/load games to EEPROM memory
- **WiFi Management**: Dynamic WiFi configuration with fallback AP mode
- **Built-in Timer**: Chess-style clock for timed games
- **Web Accessibility**: Access via IP address or `http://MagicPoints.local`

## Hardware Requirements

| Component | ESP8266 Pin | Purpose |
|-----------|-------------|---------|
| 20x4 I2C LCD | SDA→D2, SCL→D1 | Display interface |
| Plus Button | D6 | Increment values |
| Minus Button | D5 | Decrement values |  
| Main Button | D8 | Navigation/Enter |

**Note:** Use external pull-down resistors for buttons (ESP8266 doesn't have reliable INPUT_PULLDOWN).

## Physical Button Functions

### Basic Navigation
- **Main Button (D8) - Short Press**: Navigate through options (CH = Change)
- **Main Button (D8) - Long Press**: Select/Enter current option (ENT = Enter)
- **Plus + Minus Together**: Access utility options (OPT = Options)

### In Main View
- **Plus/Minus**: Adjust selected player's main points
- **Main Button (Short)**: Cycle through players only
- **Main Button (Long)**: Enter detail view for selected player OR access utilities
- **Plus + Minus Together**: Toggle between players and utilities menu

### In Detail View (Commander Damage)
- **Plus/Minus**: Add/remove damage from selected opponent
- **Main Button (Short)**: Cycle through opponents
- **Main Button (Long)**: Return to main view

### In Utilities Menu
Navigate with **Main Button (Short)**, select with **Main Button (Long)**:
- **Die Roll**: Randomize starting player
- **Reset 20/40**: Reset all players to 20 or 40 life
- **SAVE/LOAD**: Store/retrieve game state from memory
- **CLOCK**: Access chess-style timer
- **BACK**: Return to main view

### In Clock Mode
- **Plus/Minus**: Switch between Player 1 and Player 2 timers
- **Main Button (Short)**: Pause/resume current timer
- **Plus + Minus Together**: Reset both timers
- **Main Button (Long)**: Exit clock mode

## WiFi Configuration

### First Time Setup
1. If no WiFi networks are configured, device creates AP: **Points_Setup** (password: **Points123**)
2. Connect to this network and navigate to the displayed IP address
3. Use the WiFi configuration page to add your networks
4. Device will automatically connect to saved networks on next boot

### Web Interface Features
- **Game Setup**: Create games with custom player names and starting points
- **Remote Control**: All physical button functions available via web
- **WiFi Management**: Add/remove WiFi networks
- **Device Control**: Reboot device remotely

## Installation & Setup

### 1. Delete Old Files
**Important**: Before compiling, delete `source_old.ino` from the source folder. This is an outdated version that may cause compilation conflicts.

### 2. Install Arduino Libraries
```
LiquidCrystal_I2C
ESP8266WiFi  
ESP8266WebServer
ESP8266mDNS
EEPROM
```

### 3. Upload Code
1. Open `source/source.ino` in Arduino IDE
2. Select your ESP8266 board (NodeMCU, Wemos D1 Mini, etc.)
3. Upload the firmware

### 4. Initial Connection
1. Power the device
2. If WiFi networks are configured, it will connect automatically
3. If not, connect to **Points_Setup** access point
4. Configure your WiFi via the web interface

## System States & Navigation

```
Boot Screen → Connection Screen → Main Game View
                                      ↓
              ┌─────────────────── Main View ←─────────────────┐
              ↓                       ↓                       ↓
         Detail View              Utils Menu              Clock Mode
         (Per-player             (Game options)          (Timer system)
          damage tracking)
```

## Data Storage

The system automatically stores:
- Player names and points
- WiFi network configurations  
- Game state persistence across power cycles

Access via **Utils Menu → SAVE/LOAD** to manually manage game states.

---

Perfect for tracking life totals in Magic: The Gathering, victory points in board games, or any multi-player scoring scenario! 🎲📟
