@echo off
echo Starting Forza Horizon Telemetry GUI...
echo.
echo Make sure:
echo 1. Arduino is connected and sketch is uploaded
echo 2. Forza Horizon telemetry is configured (IP: 127.0.0.1, Port: 12345)
echo 3. Windows Firewall allows the application
echo.
pause
cd output
forza_to_arduino_gui_simple.exe
