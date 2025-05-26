#!/usr/bin/env python3
"""
Forza Horizon Telemetry Integration for Motion Platform Simulator - Arduino Bridge

This script captures UDP telemetry data from Forza Horizon 4/5, calculates G-forces,
and forwards them to an Arduino (e.g., Mega 2560) via USB Serial.

Setup Instructions:
1. Install Python dependencies: pip install pyserial
2. In Forza Horizon, go to Settings > HUD and Gameplay > Data Out
3. Set Data Out to ON
4. Set Data Out IP Address to 127.0.0.1 (localhost)
5. Set Data Out IP Port to 12345 (or as configured below)
6. Set Data Out Packet Format to "Car Dash"
7. Connect your Arduino running the companion sketch.
8. Update SERIAL_PORT below to match your Arduino's COM port.
9. Run this script: python forza_to_arduino_pc_app.py
10. Start racing in Forza Horizon!

The script will send G-force data like "long,lat,vert\\n" to the Arduino.
"""

import socket
import struct
# import requests # Removed
import serial # Added
import time
import math
import sys

# Configuration
FORZA_UDP_IP = "127.0.0.1"
FORZA_UDP_PORT = 12345
# SIMULATOR_API_URL = "http://localhost:3002/api/controls"  # Removed

PULL_RATE = 1  # How often to send data to Arduino (in seconds)

# Serial Configuration for Arduino
SERIAL_PORT = "COM6"  # !!! IMPORTANT: CHANGE THIS to your Arduino's serial port !!!
                      # Examples: "COM3" on Windows, "/dev/ttyUSB0" or "/dev/ttyACM0" on Linux
SERIAL_BAUD_RATE = 115200 # Must match the BAUD_RATE in your Arduino sketch

# Forza packet format - 324 bytes total, first 308 bytes are 77 floats
FORZA_PACKET_FORMAT = "<" + "f" * 77

class ForzaTelemetryProcessor:
    def __init__(self):
        # Create and configure UDP socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            self.sock.bind((FORZA_UDP_IP, FORZA_UDP_PORT))
        except OSError as e:
            print(f"[ERR] Cannot bind UDP to {FORZA_UDP_IP}:{FORZA_UDP_PORT}")
            print(f"[ERR] Error: {e}")
            print(f"[FIX] Close any other instances of this script or apps using this port and try again")
            raise
            
        self.sock.settimeout(1.0) # Timeout for UDP socket
        
        # Initialize Serial connection to Arduino
        self.ser = None
        try:
            print(f"[SERIAL] Attempting to connect to Arduino on {SERIAL_PORT} at {SERIAL_BAUD_RATE} baud...")
            self.ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD_RATE, timeout=0.1) # Short write timeout
            time.sleep(2) # Give Arduino time to reset if DTR is enabled by pyserial
            if self.ser.is_open:
                print(f"[SERIAL] Successfully connected to Arduino on {SERIAL_PORT}")
            else:
                # This case might not be reached if serial.Serial throws an exception first
                print(f"[ERR] Failed to open serial port {SERIAL_PORT}, though no exception was thrown.")
                raise serial.SerialException(f"Failed to open {SERIAL_PORT}")
        except serial.SerialException as e:
            print(f"[ERR] Cannot connect to Arduino on {SERIAL_PORT}: {e}")
            print(f"[FIX] Ensure Arduino is connected, correct COM port is selected, and drivers are installed.")
            if self.sock: self.sock.close() # Close UDP socket if serial fails
            raise

        # Initialize tracking variables
        self.prev_velocity = [0.0, 0.0, 0.0] # Retained if needed for future advanced calculations
        self.prev_time = time.time() # Retained if needed
        
        print(f"[RACE] Forza Horizon Telemetry to Arduino Bridge Started")
        print(f"[NET] Listening for Forza UDP on {FORZA_UDP_IP}:{FORZA_UDP_PORT}")
        # print(f"[API] Forwarding to {SIMULATOR_API_URL}") # Removed
        print(f"[SERIAL] Forwarding G-Force data to Arduino on {SERIAL_PORT}")
        print(f"[GO!] Start driving in Forza to see G-force data!\n")

    def parse_telemetry_packet(self, data):
        """Parse Forza telemetry UDP packet."""
        try:
            if len(data) < 308: # Based on "f"*77
                # print(f"Warning: Packet too small {len(data)}") # Optional: for debugging
                return None
                
            values = struct.unpack(FORZA_PACKET_FORMAT, data[:308])
            
            # Extract telemetry values based on Forza Horizon Car Dash format
            return {
                'current_engine_rpm': values[4],     # Current engine RPM
                'accel_x': values[5],                # Lateral acceleration (m/s²)
                'accel_y': values[6],                # Vertical acceleration (m/s²) 
                'accel_z': values[7],                # Longitudinal acceleration (m/s²)
                'velocity_x': values[8],             # Velocity X (m/s)
                'velocity_y': values[9],             # Velocity Y (m/s)
                'velocity_z': values[10],            # Velocity Z (m/s)
                'speed': math.sqrt(values[8]**2 + values[9]**2 + values[10]**2),  # Speed in m/s
                'speed_mph': math.sqrt(values[8]**2 + values[9]**2 + values[10]**2) * 2.23694, # Speed in MPH
            }
            
        except (struct.error, IndexError) as e:
            print(f"[ERR] Error parsing packet: {e}")
            return None

    def calculate_g_forces(self, telemetry):
        """Convert Forza telemetry to G-force values for motion platform."""
        
        G_FORCE = 9.81  # Standard gravity
        
        # Get raw accelerations from Forza
        accel_x = telemetry.get('accel_x', 0)    # Lateral (left/right)
        accel_y = telemetry.get('accel_y', 0)    # Vertical (up/down)
        accel_z = telemetry.get('accel_z', 0)    # Longitudinal (forward/backward)
        
        # Convert to G-forces
        g_longitudinal = -accel_z / G_FORCE      # Negative for correct direction (braking = positive G)
        g_lateral = accel_x / G_FORCE            # Left/right G-forces
        g_vertical = (accel_y / G_FORCE) + 1.0   # Add 1G for gravity baseline
        
        # Apply realistic limits for motion platform safety
        g_longitudinal = max(-3.0, min(3.0, g_longitudinal))  # ±3G limit
        g_lateral = max(-3.0, min(3.0, g_lateral))            # ±3G limit
        g_vertical = max(-1.0, min(4.0, g_vertical))          # -1G to +4G limit
        
        return {
            'longitudinal': round(g_longitudinal, 3),
            'lateral': round(g_lateral, 3),
            'vertical': round(g_vertical, 3),
            'timestamp': int(time.time() * 1000)
        }

    def send_to_arduino(self, g_forces): # Renamed from send_to_simulator
        """Send G-force data to Arduino via Serial."""
        if not self.ser or not self.ser.is_open:
            # print("[ERR] Serial port not open. Cannot send to Arduino.") # Optional: for debugging
            return False
        try:
            # Format: "long,lat,vert\n"
            data_string = f"{g_forces['longitudinal']},{g_forces['lateral']},{g_forces['vertical']}\\n"
            self.ser.write(data_string.encode('ascii'))
            return True
            
        # except requests.exceptions.RequestException: # Removed
        # return False # Removed
        except serial.SerialTimeoutException:
            print(f"[WARN] Serial write timeout to {self.ser.port}. Arduino might not be ready.")
            return False
        except Exception as e:
            print(f"[ERR] Error writing to serial port {self.ser.port}: {e}")
            return False

    def run(self):
        """Main loop to process Forza telemetry and forward to Arduino."""
        
        packet_count = 0
        last_status_time = time.time()
        last_data_time = time.time()
        
        print(f"[WAIT] Waiting for Forza Horizon telemetry data...")
        
        try:
            while True:
                try:
                    # Receive UDP packet from Forza
                    data, addr = self.sock.recvfrom(1024)
                    packet_count += 1
                    last_data_time = time.time()
                    
                    # First packet confirmation
                    if packet_count == 1:
                        print(f"[SUCCESS] Connected to Forza! Receiving {len(data)}-byte packets from {addr}")
                    
                    # Parse telemetry data
                    telemetry = self.parse_telemetry_packet(data)
                    if not telemetry:
                        continue
                    
                    # Check if car is moving (speed > 1 km/h or engine > idle)
                    speed_kmh = telemetry.get('speed', 0) * 3.6
                    rpm = telemetry.get('current_engine_rpm', 0)
                    is_active = speed_kmh > 1.0 or rpm > 1000
                    
                    if is_active:
                        # Calculate and send real G-forces when moving
                        g_forces = self.calculate_g_forces(telemetry)
                        success = self.send_to_arduino(g_forces) # Updated call
                    else:
                        # Send neutral G-forces when stopped
                        g_forces = {
                            'longitudinal': 0.0,
                            'lateral': 0.0,
                            'vertical': 1.0,  # Just gravity
                            'timestamp': int(time.time() * 1000)
                        }
                        success = self.send_to_arduino(g_forces) # Updated call
                    
                    # Status display every 0.2 seconds (5Hz)
                    current_time = time.time()
                    if current_time - last_status_time >= PULL_RATE:
                        status_icon = "🟢" if success else "🔴"
                        activity = "ACTIVE" if is_active else "IDLE"
                        speed_mph_val = telemetry.get('speed_mph', 0) # Get MPH for printing
                        
                        print(f"{status_icon} {activity} | Speed: {speed_mph_val:5.1f} mph ({speed_kmh:6.1f} km/h) | "
                              f"RPM: {rpm:4.0f} | "
                              f"G-Forces: Long:{g_forces['longitudinal']:+5.2f} "
                              f"Lat:{g_forces['lateral']:+5.2f} "
                              f"Vert:{g_forces['vertical']:+5.2f}")
                        
                        last_status_time = current_time
                        
                except socket.timeout:
                    # Handle no data gracefully
                    current_time = time.time()
                    if current_time - last_data_time > 10.0:
                        print(f"[WAIT] No telemetry data for 10+ seconds")
                        print(f"       Make sure Forza Data Out is enabled (Port {FORZA_UDP_PORT})")
                        last_data_time = current_time
                    continue
                    
                except Exception as e:
                    print(f"[ERR] Processing error: {e}")
                    continue
                    
        except KeyboardInterrupt:
            print(f"\n[STOP] Shutting down telemetry bridge...")
            
        finally:
            if self.sock:
                self.sock.close()
            if self.ser and self.ser.is_open:
                self.ser.close()
                print(f"[SERIAL] Serial port {SERIAL_PORT} closed.")
            print(f"[BYE] Telemetry bridge stopped.")

def main():
    """Main entry point."""
    print("=" * 60)
    print("🏎️  FORZA HORIZON → MOTION PLATFORM BRIDGE")
    print("=" * 60)
    
    try:
        processor = ForzaTelemetryProcessor()
        processor.run()
    except Exception as e:
        print(f"[FATAL] Failed to start: {e}")
        print(f"[FIX] Make sure port {FORZA_UDP_PORT} is available and try again")

if __name__ == "__main__":
    main()
