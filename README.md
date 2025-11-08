# ESP8266 Points Counter v2.0
A comprehensive scoreboard system for tabletop games, featuring both physical buttons and web interface control.

## What This Project Does

This ESP8266-based device creates a **digital scoreboard** perfect for tabletop games like Magic: The Gathering, board games, or any scenario where you need to track multiple players' scores. The system displays player information on a 20x4 LCD screen and provides both **physical button controls** and a **web interface** for managing the game.

### Key Features

- **Dual Interface**: Control via physical buttons or web browser
- **Multi-Player Support**: Track up to 4 players simultaneously (6-character names)
- **Advanced Scoring**: Main points + detailed commander damage tracking per opponent
- **Data Persistence**: Save/load games to EEPROM memory
- **WiFi Management**: Dynamic WiFi configuration with automatic connection and fallback AP mode
- **Built-in Timer**: Chess-style clock for timed games with pause/resume functionality
- **Web Accessibility**: Access via IP address or `http://MagicPoints.local` (mDNS)
- **Network Scanning**: Built-in WiFi scanner to discover available networks
- **Responsive Design**: Mobile-friendly web interface with haptic feedback

## Hardware Requirements

| Component | ESP8266 Pin | Purpose |
|-----------|-------------|---------|
| 20x4 I2C LCD (0x27) | SDA→D2, SCL→D1 | Display interface |
| Plus Button | D6 | Increment values |
| Minus Button | D5 | Decrement values |  
| Main Button | D8 | Navigation/Enter |

**Important Notes:** 
- Use external pull-down resistors for buttons (ESP8266 doesn't have reliable INPUT_PULLDOWN)
- LCD address is set to 0x27 (configurable in code)
- All buttons should connect to 3v3 when pressed

## Physical Button Functions

### Basic Navigation
- **Main Button (D8) - Short Press**: Navigate through options (CH = Change)
- **Main Button (D8) - Long Press**: Select/Enter current option (ENT = Enter)
- **Plus + Minus Together**: Access utility options (OPT = Options)

### In Main View
- **Plus/Minus**: Adjust selected player's main points
- **Main Button (Short)**: Cycle through players only
- **Main Button (Long)**: Enter detail view for selected player
- **Plus + Minus Together**: Toggle between players and utilities menu

### In Detail View (Commander Damage)
Shows detailed damage tracking for the selected player against each opponent:
- **Display**: "Comm.Dmg [Player] P:[Points]" with damage breakdown
- **Plus/Minus**: Add/remove damage from selected opponent (adjusts main points automatically)
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
Chess-style timer system for timed games:
- **Display**: Shows P1 time, P2 time, and total game time (MM:SS format)
- **Plus/Minus**: Switch active timer between Player 1 and Player 2
- **Main Button (Short)**: Pause/resume active timer (hourglass icon shows when paused)
- **Plus + Minus Together**: Reset both timers to 00:00
- **Main Button (Long)**: Exit clock mode and return to main view (clock is stopped)

## WiFi Configuration & Modes

### Automatic Connection Mode
- Device attempts to connect to saved WiFi networks on startup
- Successfully connected devices show:
  - Connected network SSID
  - Device IP address
  - Status: "Connect to this wifi to start a game"

### Configuration Mode (Access Point)
When no saved networks exist or connection fails:
1. Device creates AP: **Points_Setup** (password: **Points123**)
2. AP operates on channel 6 with optimized settings for better performance
3. Device IP: Usually `192.168.4.1`
4. LCD shows: "WiFi Setup Mode" with connection details

### Network Management Features
- **Up to 3 saved networks**: Automatically tries all saved networks
- **Network scanning**: Web interface includes WiFi scanner
- **One-click setup**: Click scanned networks to auto-populate SSID
- **Secure storage**: WiFi credentials stored in EEPROM with validation
- **Automatic failover**: Falls back to AP mode if connections fail

## Web Interface Features

### Game Management
- **Create Games**: Set up to 4 players with custom names (max 6 characters) and starting points
- **Live Control**: All physical button functions available remotely

### WiFi Management  
- **Network Scanner**: Discover and display available WiFi networks with signal strength
- **Easy Addition**: Click scanned networks to auto-populate SSID field
- **Network List**: View all saved networks with delete options
- **Secure Addition**: Add networks with SSID and password
- **Remote Reboot**: Restart device from web interface

### Mobile Optimization
- **Responsive Design**: Works on phones, tablets, and computers
- **Haptic Feedback**: Button presses trigger vibration on supported devices
- **Large Buttons**: Easy-to-press controls for game management
- **Quick Actions**: Direct +/- buttons and function shortcuts

## Installation & Setup

### 1. Required Arduino Libraries
Install these libraries through the Arduino IDE Library Manager:
```
LiquidCrystal_I2C by Frank de Brabander
ESP8266WiFi (included with ESP8266 board package)
ESP8266WebServer (included with ESP8266 board package)  
ESP8266mDNS (included with ESP8266 board package)
EEPROM (included with ESP8266 board package)
```

### 2. Hardware Configuration
- **ESP8266 Board**: NodeMCU, Wemos D1 Mini, or similar
- **Important**: Install external pull-down resistors (10kΩ) on button pins D5, D6, D8
- **LCD**: Ensure I2C address is 0x27 (most common) or modify `LCD_ADDRESS` in code

### 3. Code Upload
1. **Clean workspace**: Delete `source_old.ino` if present (outdated version)
2. Open `source/source.ino` in Arduino IDE
3. Select appropriate ESP8266 board and port
4. Verify compilation settings:
   - Flash Size: 4MB (FS: 2MB OTA: ~1019KB)
   - CPU Frequency: 80 MHz
5. Upload the firmware

### 4. First Boot & Setup
1. Power the device and wait for boot sequence
2. **Default game setup**: Long-press main button from connection screen to create default 4-player game
3. **WiFi configuration**: 
   - If no networks saved: Connect to "Points_Setup" AP and configure WiFi
   - If networks saved: Device connects automatically and shows IP address

## System States & Navigation Flow

```
Boot Screen → WiFi Connection → Connection Display → Main Game View
                                                           ↓
              ┌─────────────────── Main View ←─────────────────┐
              ↓                       ↓                       ↓
         Detail View              Utils Menu              Clock Mode
      (Commander damage          (Game management)        (Timer system)
       per opponent)              • Die Roll              • P1/P2 timers
                                  • Reset 20/40           • Pause/Resume
                                  • Save/Load             • Reset function
                                  • Clock access
                                  • Back to main
```

## Display Features & Visual Indicators

### Custom LCD Characters
- **→** Right arrow: Indicates selected player/option
- **←** Left arrow: Indicates active selection in menus
- **⧗** Hourglass: Shows during die roll animation and when clock is paused

### Screen Layout
- **Main View**: Player names (6 chars max), main points, "Utils" indicator
- **Detail View**: "Comm.Dmg [Player] P:[Points]" with damage breakdown
- **Utils Menu**: 2x3 grid layout with navigation indicators
- **Clock View**: "CLOCK" header, P1/P2 times, total time display

## Data Storage & Persistence

### EEPROM Configuration
The system uses ESP8266's EEPROM (512 bytes total) for persistent storage:

**Game Data (Addresses 0-255)**
- Magic number validation (0xFEDE)
- Player names (4 players × 7 bytes each = 28 bytes)
- Points matrix (4 players × 4 opponents × 1 byte = 16 bytes)
- Number of active players (1 byte)

**WiFi Data (Addresses 256-511)**  
- WiFi magic number validation (0xC0DE)
- Network count (1 byte)
- Network configurations (3 networks × 66 bytes each = 198 bytes)
  - SSID (33 bytes including null terminator)
  - Password (33 bytes including null terminator)

### Save/Load Behavior
- **WiFi settings**: Automatically saved when networks are added/removed
- **Game data**: Manual save/load only via Utils Menu → SAVE/LOAD
- **Automatic validation**: Both WiFi and game data include magic numbers for corruption detection
- **Graceful fallback**: Invalid data results in default values, not crashes

**Important**: Game progress is not automatically saved during play. Use manual SAVE function to preserve game state between power cycles.

## Technical Specifications

### Performance & Timing
- **Main loop**: 15ms cycle time for responsive button handling
- **Display updates**: 200ms refresh rate or when changes detected. As fast as possible refresh rate on clock screen
- **Button debounce**: 150ms debounce with 350ms long-press threshold (adjustable in code)
- **WiFi timeout**: 15 seconds connection attempt before fallback to AP mode

### Memory Management
- **EEPROM**: 512 bytes total (256 for game data, 256 for WiFi config)
- **Web pages**: Stored in PROGMEM to preserve RAM
- **String handling**: Efficient memory allocation with reserve() for large operations
- **WiFi optimization**: Channel 6 selection and power management for AP mode

### Network Features
- **mDNS support**: Access via `http://MagicPoints.local` when connected to WiFi
- **Concurrent handling**: Web server and button inputs processed simultaneously
- **Response optimization**: Minimal JSON responses for mobile performance
- **Security**: WPA2 encryption for both client and AP modes

---

## Troubleshooting

### Common Issues
1. **Display not working**: Check I2C connections and verify LCD address (0x27)
2. **Buttons not responsive**: Ensure pull-down resistors are installed correctly  
3. **WiFi connection fails**: Use AP mode to reconfigure networks
4. **Memory corruption**: Power cycle device to reset EEPROM validation

### LED Status Indicators
- **LED ON**: Active web request being processed
- **LED OFF**: Normal operation
- **LED patterns**: Can indicate WiFi connection status during boot

---

Perfect for tracking life totals in Magic: The Gathering, victory points in board games, or any multi-player scoring scenario! 🎲📟🎮
