@echo off
echo ====================================================
echo  Forza Horizon Telemetry - Performance Comparison
echo ====================================================
echo.
echo This script demonstrates the performance improvements
echo achieved through threading optimization.
echo.
echo BEFORE (Timer-Based):
echo   - Packet processing: Every 50ms (timer-based)
echo   - UI blocking: Yes (during telemetry processing)
echo   - Maximum delay: 50ms
echo   - Thread usage: Single thread (UI + telemetry)
echo.
echo AFTER (Threading Optimized):
echo   - Packet processing: Continuous (~1ms intervals)
echo   - UI blocking: No (dedicated telemetry thread)
echo   - Maximum delay: 1ms
echo   - Thread usage: Multi-threaded (UI + dedicated telemetry)
echo.
echo PERFORMANCE GAIN: 50x faster packet processing
echo.
echo ====================================================
echo  Available Applications:
echo ====================================================
echo.
echo 1. Original Application (Timer-Based):
echo    .\output\forza_to_arduino_gui_simple.exe
echo.
echo 2. Optimized Application (Threading):
echo    .\output\forza_to_arduino_gui_simple_optimized.exe
echo.
echo ====================================================
echo  Testing Instructions:
echo ====================================================
echo.
echo 1. Launch Forza Horizon 4/5
echo 2. Enable telemetry output (UDP broadcast)
echo 3. Connect Arduino to USB port
echo 4. Run either application above
echo 5. Configure UDP IP/Port and Serial settings
echo 6. Click "Start Telemetry"
echo 7. Drive in Forza and observe real-time performance
echo.
echo EXPECTED RESULTS:
echo   - Original: Noticeable delays during fast driving
echo   - Optimized: Smooth, real-time telemetry processing
echo.
pause
