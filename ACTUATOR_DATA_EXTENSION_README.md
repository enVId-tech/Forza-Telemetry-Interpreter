# Forza Horizon Telemetry Extension - Actuator Data Support

## Overview
This extension adds actuator data capture and transmission to the existing Forza Horizon telemetry system. The system now captures and displays brake, throttle, steering wheel, and suspension data along with the existing G-force data.

## Changes Made

### 1. C++ GUI Application (`forza_to_arduino_gui_simple.cpp`)

#### Extended TelemetryData Structure
- Added actuator data fields:
  - `uint8_t throttle` - Throttle input (0-255, displayed as 0-100%)
  - `uint8_t brake` - Brake input (0-255, displayed as 0-100%)
  - `int8_t steering` - Steering wheel position (-255 to +255)
  - `float suspension_fl/fr/rl/rr` - Normalized suspension travel (0.0-1.0)

#### Updated Telemetry Parsing
- Enhanced `parse_telemetry_packet()` to extract actuator data from Forza packet:
  - Suspension travel from float indices 17-20 (based on Forza packet structure research)
  - Throttle/brake/steering from byte data at approximate offset 232

#### Extended GUI Display
- Added new window controls to display actuator data:
  - Throttle percentage
  - Brake percentage
  - Steering wheel position
  - Suspension travel for all four wheels

#### Updated Serial Communication
- Extended serial protocol to send 10 values instead of 3:
  - Old format: `longitude,latitude,vertical\n`
  - New format: `longitude,latitude,vertical,throttle,brake,steering,suspension_fl,suspension_fr,suspension_rl,suspension_rr\n`

### 2. Arduino Sketch (`main/main.ino`)

#### Extended Processing Function
- Updated `processGForceData()` to accept actuator data parameters
- Enhanced status logging to display all telemetry values

#### Updated Parsing Logic
- Modified main loop to parse 10-value format
- Maintained backward compatibility with 3-value format
- Added proper error handling for parsing failures

#### Improved Status Display
- Extended serial output to show:
  - G-force values (longitude, lateral, vertical)
  - Actuator data (throttle %, brake %, steering position)
  - Suspension travel values (FL, FR, RL, RR)
  - PWM motor control values

## File Changes

### Modified Files:
1. `forza_to_arduino_gui_simple.cpp` - Main GUI application
2. `main/main.ino` - Arduino sketch

### New Executable:
- `output/forza_to_arduino_gui_extended.exe` - Extended GUI application

## Data Format

### Serial Communication Protocol
```
longitude,latitude,vertical,throttle,brake,steering,suspension_fl,suspension_fr,suspension_rl,suspension_rr\n
```

### Example Data:
```
-0.150,0.230,1.050,85.5,12.3,-45,0.25,0.32,0.18,0.41
```

Where:
- `-0.150` = Longitudinal G-force
- `0.230` = Lateral G-force  
- `1.050` = Vertical G-force
- `85.5` = Throttle percentage
- `12.3` = Brake percentage
- `-45` = Steering position
- `0.25,0.32,0.18,0.41` = Suspension travel (FL,FR,RL,RR)

## Forza Telemetry Packet Mapping

Based on research from external Forza telemetry repositories:

### Float Values (indices in 77-float array):
- `values[17]` = NormalizedSuspensionTravelFrontLeft
- `values[18]` = NormalizedSuspensionTravelFrontRight
- `values[19]` = NormalizedSuspensionTravelRearLeft
- `values[20]` = NormalizedSuspensionTravelRearRight

### Byte Values (approximate byte offset 232):
- `byte_data[232 + 0]` = Throttle/Accel (0-255)
- `byte_data[232 + 1]` = Brake (0-255)
- `byte_data[232 + 4]` = Steering (-255 to +255)

**Note:** These byte offsets may need adjustment based on the exact Forza packet structure for your game version.

## Testing Instructions

### 1. Setup
1. Upload the updated Arduino sketch to your Arduino Mega 2560
2. Run the extended GUI application: `output/forza_to_arduino_gui_extended.exe`
3. Configure UDP port (default: 12345) and serial port (default: COM6)

### 2. Forza Horizon Configuration
1. Enable telemetry data output in Forza Horizon settings
2. Set telemetry IP to `127.0.0.1` (localhost)
3. Set telemetry port to `12345` (or your configured port)

### 3. Verification
1. Start the telemetry application
2. Launch Forza Horizon and begin driving
3. Monitor the GUI for actuator data updates
4. Check Arduino Serial Monitor (115200 baud) for received data

### 4. Expected Behavior
- GUI should display real-time updates of throttle, brake, steering, and suspension data
- Arduino should log detailed telemetry information including actuator values
- All data should update smoothly during gameplay

## Backward Compatibility

The Arduino sketch maintains backward compatibility with the old 3-value format. If the old C++ application or Python script is used, the Arduino will:
- Parse only the G-force values
- Set actuator data to default values (0)
- Continue to function normally

## Troubleshooting

### Common Issues:

1. **No Actuator Data Updates**
   - Verify Forza is sending telemetry data
   - Check that packet size is correct (308 bytes minimum)
   - Verify actuator data byte offsets are correct for your Forza version

2. **Parsing Errors on Arduino**
   - Check serial communication format
   - Verify baud rate (115200)
   - Monitor Arduino Serial Monitor for error messages

3. **GUI Compilation Issues**
   - Ensure all required headers are included
   - Use proper compiler flags: `-lws2_32 -lcomctl32`
   - Check for Windows macro conflicts with std::min/max

### Packet Offset Calibration

If actuator data appears incorrect, the byte offsets may need adjustment:

1. Use a packet analyzer to determine exact structure
2. Modify the offset values in `parse_telemetry_packet()`
3. Test with known input values (e.g., full throttle, full brake)
4. Adjust offsets until data matches expected values

## Future Enhancements

Potential improvements:
1. Add gear position display
2. Include wheel slip data
3. Add engine temperature monitoring
4. Implement lap time tracking
5. Add configuration file for packet offsets
6. Create real-time data logging functionality

## Technical Notes

- Uses Windows API for GUI (compatible with older MinGW versions)
- Employs non-blocking UDP socket for telemetry reception
- Implements proper error handling and timeout detection
- Maintains thread-safe UI updates via timer-based approach
- Includes proper resource cleanup for network and serial connections
