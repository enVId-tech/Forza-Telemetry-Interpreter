# Note: Made with AI

# Forza Horizon Telemetry → Arduino Bridge

A Windows application that captures UDP telemetry data from Forza Horizon 4/5, calculates G-forces, and forwards them to an Arduino via USB Serial. Available in both console and GUI versions.

## ⚡ Performance Optimization (NEW!)

**Threading-optimized version available!** The latest optimized GUI application delivers **50x faster** packet processing:
- ✅ **Real-time telemetry processing** - No more driving delays
- ✅ **Dedicated threading** - Continuous packet polling instead of timer-based processing  
- ✅ **Non-blocking UI** - Interface remains responsive during heavy telemetry processing
- ✅ **1ms processing intervals** - Down from 50ms timer-based delays

**Use the optimized version:** `.\output\forza_to_arduino_gui_simple_optimized.exe`

📊 **[View detailed performance report](THREADING_OPTIMIZATION_REPORT.md)**

## 🚀 Quick Start

### 1. Prerequisites
- **Forza Horizon 4 or 5** with telemetry output enabled
- **Arduino** (tested with Mega 2560) connected via USB
- **Windows** with MinGW/GCC compiler (for building from source)

### 2. Arduino Setup
1. Upload the provided `main/main.ino` sketch to your Arduino
2. Note the COM port (e.g., COM6) in Device Manager
3. Ensure baud rate matches (default: 115200)

### 3. Forza Horizon Setup
1. Go to **Settings → HUD and Gameplay**
2. Enable **Data Out** 
3. Set **Data Out IP Address**: `127.0.0.1` (localhost)
4. Set **Data Out IP Port**: `12345`
5. Set **Data Out Packet Format**: `Car Dash`

## 📱 GUI Application

### Available Versions

#### 🔥 Optimized Version (Recommended)
```bash
# High-performance threading-optimized version
.\output\forza_to_arduino_gui_simple_optimized.exe
```
- **50x faster packet processing**
- **Real-time telemetry** with 1ms intervals
- **Non-blocking UI** - Dedicated telemetry thread
- **No driving delays** - Continuous packet polling

#### 📱 Standard Version
```bash
# Original timer-based version
.\output\forza_to_arduino_gui_simple.exe
```
- Timer-based processing (50ms intervals)
- May experience delays during fast driving
- Single-threaded operation

### Running the GUI Version
```bash
# Use the optimized version for best performance
.\output\forza_to_arduino_gui_simple_optimized.exe
```

### GUI Features
- **Real-time telemetry display**: Speed, RPM, G-forces
- **Configurable settings**: IP, port, serial port, baud rate, update rate
- **Start/Stop controls** with status indicators
- **Activity monitoring**: Shows when car is active vs idle
- **Packet counter**: Tracks received telemetry packets

### GUI Configuration
| Setting | Default | Description |
|---------|---------|-------------|
| UDP IP | 127.0.0.1 | IP address to listen for Forza telemetry |
| UDP Port | 12345 | Port number for telemetry data |
| Serial Port | COM6 | Arduino COM port |
| Baud Rate | 115200 | Serial communication speed |
| Update Rate | 1.0 | Seconds between updates |

### Using the GUI
1. **Configure settings** in the left panel (IP, port, serial port, etc.)
2. Click **"Start Telemetry"** to begin
3. **Start Forza Horizon** and begin driving
4. **Monitor real-time data** in the right panel:
   - Speed in mph and km/h
   - Engine RPM
   - G-forces (longitudinal, lateral, vertical)
   - Connection status and activity

## 🖥️ Console Application

### Running the Console Version
```bash
# From the output directory
.\forza_to_arduino_pc_app.exe
```

The console version uses hardcoded settings and provides text-based status updates.

## 📊 Telemetry Data

### G-Force Calculations
- **Longitudinal**: Forward/backward forces (acceleration/braking)
- **Lateral**: Left/right forces (turning)
- **Vertical**: Up/down forces (bumps, jumps)

### Safety Limits
- Longitudinal: ±3.0G
- Lateral: ±3.0G  
- Vertical: -1.0G to +4.0G

### Data Format Sent to Arduino
```
longitude,latitude,vertical\n
Example: -0.510,0.204,1.000\n
```

## 🛠️ Building from Source

### Requirements
- MinGW-w64 or similar GCC compiler
- Windows SDK (for Windows API)

### Compilation
```bash
# GUI version (recommended)
g++ -std=c++17 -O2 -Wall forza_to_arduino_gui_simple.cpp -o output/forza_to_arduino_gui_simple.exe -lws2_32 -lcomctl32 -lgdi32 -luser32 -lkernel32

# Console version  
g++ -std=c++17 -O2 -Wall forza_to_arduino_pc_app.cpp -o output/forza_to_arduino_pc_app.exe -lws2_32

# Test program
g++ -std=c++17 -O2 test_telemetry.cpp -o output/test_telemetry.exe
```

## 🔧 Troubleshooting

### Common Issues

**"Failed to initialize serial connection"**
- Check Arduino is connected and COM port is correct
- Verify Arduino sketch is uploaded and running
- Try different baud rates (9600, 115200)

**"Waiting for Forza..."**
- Ensure Forza telemetry settings match application settings
- Check Windows Firewall isn't blocking the application
- Verify you're driving (not in menus)

**No G-force data**
- Ensure car is moving (speed > 1 mph or RPM > 1000)
- Check Arduino serial monitor for incoming data
- Verify serial connection is working

### Port Conflicts
If port 12345 is in use, change both:
1. Forza Horizon telemetry port setting
2. Application UDP Port setting

## 📁 File Structure

```
SRS-DataIntepreter/
├── main/
│   └── main.ino                          # Arduino sketch
├── output/
│   ├── forza_to_arduino_gui_simple.exe   # GUI application
│   ├── forza_to_arduino_pc_app.exe       # Console application
│   └── test_telemetry.exe                # Test program
├── forza_to_arduino_gui_simple.cpp       # GUI source code
├── forza_to_arduino_pc_app.cpp           # Console source code
├── forza_to_arduino.py                   # Original Python script
├── forza_to_arduino_pc_app.py            # Python version
└── test_telemetry.cpp                    # Test source code
```

## 🎮 Supported Games

- **Forza Horizon 4** ✅
- **Forza Horizon 5** ✅
- **Forza Motorsport 7** ✅ (with compatible telemetry format)

## 🔌 Arduino Compatibility

Tested with:
- Arduino Mega 2560 ✅
- Arduino Uno ✅ (limited memory)
- Arduino Nano ✅

## 📝 Notes

- The GUI version uses Windows timers instead of threads for better compatibility with older MinGW versions
- Telemetry data is only sent when the car is active (moving or engine running)
- Application automatically handles connection loss and reconnection
- G-forces are calculated from acceleration data and include safety limits

## 🚗 How It Works

1. **Forza Horizon** sends UDP packets containing telemetry data (77 float values)
2. **Application** receives and parses packets, extracting speed, RPM, and acceleration
3. **G-forces** are calculated from acceleration data using physics formulas
4. **Arduino** receives G-force data via serial and can control hardware (LEDs, motors, etc.)

## 💡 Use Cases

- **Motion simulators**: Control actuators based on G-forces
- **LED indicators**: Show braking, turning, speed via lights  
- **Haptic feedback**: Vibration motors for immersion
- **Data logging**: Record telemetry for analysis
- **Educational**: Learn about vehicle dynamics and physics
