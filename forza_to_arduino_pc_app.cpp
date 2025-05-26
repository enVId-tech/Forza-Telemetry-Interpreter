// forza_to_arduino_pc_app.cpp
/*
Forza Horizon Telemetry Integration for Motion Platform Simulator - Arduino Bridge

This script captures UDP telemetry data from Forza Horizon 4/5, calculates G-forces,
and forwards them to an Arduino (e.g., Mega 2560) via USB Serial.

Setup Instructions:
1. In Forza Horizon, go to Settings > HUD and Gameplay > Data Out
2. Set Data Out to ON
3. Set Data Out IP Address to 127.0.0.1 (localhost)
4. Set Data Out IP Port to 12345 (or as configured below)
5. Set Data Out Packet Format to "Car Dash"
6. Connect your Arduino running the companion sketch.
7. Update SERIAL_PORT below to match your Arduino's COM port.
8. Compile and run this program
9. Start racing in Forza Horizon!

The script will send G-force data like "long,lat,vert\n" to the Arduino.
*/

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <exception>
#include <thread>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// Note: For MinGW, link with -lws2_32 instead of #pragma comment

// Configuration
const char* FORZA_UDP_IP = "127.0.0.1";
const int FORZA_UDP_PORT = 12345;
const double PULL_RATE = 1.0; // How often to display status (in seconds) - matches Python

// Serial Configuration for Arduino
const char* SERIAL_PORT = "COM6"; // IMPORTANT: CHANGE THIS to your Arduino's serial port !!!
const int SERIAL_BAUD_RATE = 115200; // Must match the BAUD_RATE in your Arduino sketch

// Forza packet format - 324 bytes total, first 308 bytes are 77 floats
const int FORZA_NUM_FLOATS = 77;
const int FORZA_PACKET_SIZE_BYTES = FORZA_NUM_FLOATS * sizeof(float); // 308 bytes

struct TelemetryData {
    float current_engine_rpm;
    float accel_x; // Lateral acceleration (m/s²)
    float accel_y; // Vertical acceleration (m/s²) 
    float accel_z; // Longitudinal acceleration (m/s²)
    float velocity_x; // Velocity X (m/s)
    float velocity_y; // Velocity Y (m/s)
    float velocity_z; // Velocity Z (m/s)
    double speed; // Speed in m/s
    double speed_mph; // Speed in MPH
};

struct GForces {
    double longitudinal;
    double lateral;
    double vertical;
    long long timestamp;
};

class ForzaTelemetryProcessor {
public:
    ForzaTelemetryProcessor() : sock(INVALID_SOCKET), serial_handle(INVALID_HANDLE_VALUE), wsa_initialized(false), running(false) {
        if (!initialize_winsock()) {
            throw std::runtime_error("Failed to initialize Winsock");
        }
        if (!initialize_udp_socket()) {
            cleanup_winsock();
            throw std::runtime_error("Failed to initialize UDP socket");
        }
        if (!initialize_serial()) {
            cleanup_udp_socket();
            cleanup_winsock();
            throw std::runtime_error("Failed to initialize serial connection");
        }

        // Initialize tracking variables (matching Python)
        prev_velocity[0] = prev_velocity[1] = prev_velocity[2] = 0.0f;
        prev_time = std::chrono::high_resolution_clock::now();

        std::cout << "[RACE] Forza Horizon Telemetry to Arduino Bridge Started" << std::endl;
        std::cout << "[NET] Listening for Forza UDP on " << FORZA_UDP_IP << ":" << FORZA_UDP_PORT << std::endl;
        std::cout << "[SERIAL] Forwarding G-Force data to Arduino on " << SERIAL_PORT << std::endl;
        std::cout << "[GO!] Start driving in Forza to see G-force data!" << std::endl << std::endl;
    }

    ~ForzaTelemetryProcessor() {
        cleanup_serial();
        cleanup_udp_socket();
        cleanup_winsock();
        std::cout << "[BYE] Telemetry bridge stopped." << std::endl;
    }

    TelemetryData* parse_telemetry_packet(const char* data, int data_len) {
        try {
            if (data_len < 308) { // Based on 77 floats * 4 bytes
                // Optional: for debugging
                // std::cout << "Warning: Packet too small " << data_len << std::endl;
                return nullptr;
            }

            const float* values = reinterpret_cast<const float*>(data);
            
            static TelemetryData telemetry;
            
            // Extract telemetry values based on Forza Horizon Car Dash format
            telemetry.current_engine_rpm = values[4];  // Current engine RPM
            telemetry.accel_x = values[5];             // Lateral acceleration (m/s²)
            telemetry.accel_y = values[6];             // Vertical acceleration (m/s²) 
            telemetry.accel_z = values[7];             // Longitudinal acceleration (m/s²)
            telemetry.velocity_x = values[8];          // Velocity X (m/s)
            telemetry.velocity_y = values[9];          // Velocity Y (m/s)
            telemetry.velocity_z = values[10];         // Velocity Z (m/s)
            
            // Calculate speed in m/s and mph
            telemetry.speed = std::sqrt(
                telemetry.velocity_x * telemetry.velocity_x +
                telemetry.velocity_y * telemetry.velocity_y +
                telemetry.velocity_z * telemetry.velocity_z
            );
            telemetry.speed_mph = telemetry.speed * 2.23694; // Speed in MPH

            return &telemetry;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERR] Error parsing packet: " << e.what() << std::endl;
            return nullptr;
        }
    }

    GForces calculate_g_forces(const TelemetryData& telemetry) {
        const double G_FORCE = 9.81; // Standard gravity
        
        // Get raw accelerations from Forza
        double accel_x = telemetry.accel_x; // Lateral (left/right)
        double accel_y = telemetry.accel_y; // Vertical (up/down)
        double accel_z = telemetry.accel_z; // Longitudinal (forward/backward)
        
        // Convert to G-forces
        double g_longitudinal = -accel_z / G_FORCE;      // Negative for correct direction (braking = positive G)
        double g_lateral = accel_x / G_FORCE;            // Left/right G-forces
        double g_vertical = (accel_y / G_FORCE) + 1.0;   // Add 1G for gravity baseline
        
        // Apply realistic limits for motion platform safety
        g_longitudinal = std::max(-3.0, std::min(3.0, g_longitudinal)); // ±3G limit
        g_lateral = std::max(-3.0, std::min(3.0, g_lateral));           // ±3G limit
        g_vertical = std::max(-1.0, std::min(4.0, g_vertical));         // -1G to +4G limit
        
        // Get current timestamp in milliseconds
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        return {
            round_to_places(g_longitudinal, 3),
            round_to_places(g_lateral, 3),
            round_to_places(g_vertical, 3),
            timestamp
        };
    }

    bool send_to_arduino(const GForces& g_forces) {
        if (serial_handle == INVALID_HANDLE_VALUE) {
            // Optional: for debugging
            // std::cout << "[ERR] Serial port not open. Cannot send to Arduino." << std::endl;
            return false;
        }
        
        try {
            // Format: "long,lat,vert\\n" - Note: Python code has literal \\n
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3)
                << g_forces.longitudinal << ","
                << g_forces.lateral << ","
                << g_forces.vertical << "\\n"; // Literal \n as in Python
            
            std::string data_string = oss.str();
            
            DWORD bytes_written;
            if (!WriteFile(serial_handle, data_string.c_str(), static_cast<DWORD>(data_string.length()), &bytes_written, NULL)) {
                return false;
            }
            return bytes_written == data_string.length();
            
        } catch (const std::exception& e) {
            std::cerr << "[ERR] Error writing to serial port " << SERIAL_PORT << ": " << e.what() << std::endl;
            return false;
        }
    }

    void run() {
        int packet_count = 0;
        auto last_status_time = std::chrono::steady_clock::now();
        auto last_data_time = std::chrono::steady_clock::now();
        
        std::cout << "[WAIT] Waiting for Forza Horizon telemetry data..." << std::endl;
        
        running = true;
        try {
            while (running) {
                try {
                    // Receive UDP packet from Forza
                    char recv_buffer[1024];
                    sockaddr_in sender_addr;
                    int sender_addr_size = sizeof(sender_addr);
                    
                    int bytes_received = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0, 
                                                (SOCKADDR*)&sender_addr, &sender_addr_size);
                    
                    if (bytes_received == SOCKET_ERROR) {
                        int error = WSAGetLastError();
                        if (error == WSAETIMEDOUT) {
                            // Handle no data gracefully
                            auto current_time = std::chrono::steady_clock::now();
                            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_data_time);
                            if (elapsed.count() > 10) {
                                std::cout << "[WAIT] No telemetry data for 10+ seconds" << std::endl;
                                std::cout << "       Make sure Forza Data Out is enabled (Port " << FORZA_UDP_PORT << ")" << std::endl;
                                last_data_time = current_time;
                            }
                            continue;
                        } else {
                            std::cerr << "[ERR] recvfrom failed with error: " << error << std::endl;
                            continue;
                        }
                    }
                    
                    packet_count++;
                    last_data_time = std::chrono::steady_clock::now();                    // First packet confirmation
                    if (packet_count == 1) {
                        char* sender_ip = inet_ntoa(sender_addr.sin_addr);
                        std::cout << "[SUCCESS] Connected to Forza! Receiving " << bytes_received 
                                  << "-byte packets from " << sender_ip << ":" << ntohs(sender_addr.sin_port) << std::endl;
                    }
                    
                    // Parse telemetry data
                    TelemetryData* telemetry = parse_telemetry_packet(recv_buffer, bytes_received);
                    if (!telemetry) {
                        continue;
                    }
                    
                    // Check if car is moving (speed > 1 km/h or engine > idle)
                    double speed_kmh = telemetry->speed * 3.6;
                    double rpm = telemetry->current_engine_rpm;
                    bool is_active = speed_kmh > 1.0 || rpm > 1000;
                    
                    GForces g_forces;
                    bool success;
                    
                    if (is_active) {
                        // Calculate and send real G-forces when moving
                        g_forces = calculate_g_forces(*telemetry);
                        success = send_to_arduino(g_forces);
                    } else {
                        // Send neutral G-forces when stopped
                        auto now = std::chrono::system_clock::now();
                        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                        g_forces = {0.0, 0.0, 1.0, timestamp}; // Just gravity
                        success = send_to_arduino(g_forces);
                    }
                    
                    // Status display every PULL_RATE seconds (matches Python exactly)
                    auto current_time = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration<double>(current_time - last_status_time);
                    if (elapsed.count() >= PULL_RATE) {
                        std::string status_icon = success ? "🟢" : "🔴";
                        std::string activity = is_active ? "ACTIVE" : "IDLE";
                        double speed_mph_val = telemetry->speed_mph;
                        
                        std::cout << status_icon << " " << activity 
                                  << " | Speed: " << std::fixed << std::setw(5) << std::setprecision(1) << speed_mph_val << " mph "
                                  << "(" << std::fixed << std::setw(6) << std::setprecision(1) << speed_kmh << " km/h) | "
                                  << "RPM: " << std::fixed << std::setw(4) << std::setprecision(0) << rpm << " | "
                                  << "G-Forces: Long:" << std::showpos << std::fixed << std::setw(5) << std::setprecision(2) << g_forces.longitudinal << " "
                                  << "Lat:" << std::showpos << std::fixed << std::setw(5) << std::setprecision(2) << g_forces.lateral << " "
                                  << "Vert:" << std::showpos << std::fixed << std::setw(5) << std::setprecision(2) << g_forces.vertical
                                  << std::noshowpos << std::endl;
                        
                        last_status_time = current_time;
                    }
                    
                } catch (const std::exception& e) {
                    std::cerr << "[ERR] Processing error: " << e.what() << std::endl;
                    continue;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "\n[STOP] Shutting down telemetry bridge..." << std::endl;
        }
    }
    
    void stop() {
        running = false;
    }

private:
    SOCKET sock;
    HANDLE serial_handle;
    WSADATA wsa_data;
    bool wsa_initialized;
    float prev_velocity[3]; // Retained if needed for future advanced calculations
    std::chrono::high_resolution_clock::time_point prev_time; // Retained if needed
    std::atomic<bool> running;

    double round_to_places(double value, int places) {
        double multiplier = std::pow(10.0, places);
        return std::round(value * multiplier) / multiplier;
    }

    bool initialize_winsock() {
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            std::cerr << "[ERR] WSAStartup failed. Error Code: " << WSAGetLastError() << std::endl;
            return false;
        }
        wsa_initialized = true;
        return true;
    }

    void cleanup_winsock() {
        if (wsa_initialized) {
            WSACleanup();
            wsa_initialized = false;
        }
    }

    bool initialize_udp_socket() {
        // Create and configure UDP socket
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            std::cerr << "[ERR] Could not create socket: " << WSAGetLastError() << std::endl;
            return false;
        }

        // Set SO_REUSEADDR (matching Python's setsockopt)
        BOOL reuseAddr = TRUE;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr)) == SOCKET_ERROR) {
            std::cerr << "[WARN] setsockopt(SO_REUSEADDR) failed: " << WSAGetLastError() << std::endl;
        }        try {
            sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(FORZA_UDP_PORT);
            // Use inet_addr for MinGW compatibility
            server_addr.sin_addr.s_addr = inet_addr(FORZA_UDP_IP);

            if (bind(sock, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
                std::cerr << "[ERR] Cannot bind UDP to " << FORZA_UDP_IP << ":" << FORZA_UDP_PORT << std::endl;
                std::cerr << "[ERR] Error: " << WSAGetLastError() << std::endl;
                std::cerr << "[FIX] Close any other instances of this script or apps using this port and try again" << std::endl;
                closesocket(sock);
                sock = INVALID_SOCKET;
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERR] Socket bind error: " << e.what() << std::endl;
            closesocket(sock);
            sock = INVALID_SOCKET;
            return false;
        }

        // Set timeout for recvfrom (matching Python's settimeout(1.0))
        DWORD timeout_ms = 1000; // 1 second timeout
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
            std::cerr << "[WARN] Failed to set socket timeout: " << WSAGetLastError() << std::endl;
        }
        
        return true;
    }

    void cleanup_udp_socket() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
    }

    bool initialize_serial() {
        std::cout << "[SERIAL] Attempting to connect to Arduino on " << SERIAL_PORT 
                  << " at " << SERIAL_BAUD_RATE << " baud..." << std::endl;
        
        try {
            serial_handle = CreateFileA(SERIAL_PORT,
                                       GENERIC_READ | GENERIC_WRITE,
                                       0,
                                       NULL,
                                       OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL,
                                       NULL);

            if (serial_handle == INVALID_HANDLE_VALUE) {
                std::cerr << "[ERR] Cannot connect to Arduino on " << SERIAL_PORT << ": " << GetLastError() << std::endl;
                std::cerr << "[FIX] Ensure Arduino is connected, correct COM port is selected, and drivers are installed." << std::endl;
                if (sock != INVALID_SOCKET) {
                    closesocket(sock); // Close UDP socket if serial fails
                    sock = INVALID_SOCKET;
                }
                return false;
            }            DCB dcb_serial_params;
            memset(&dcb_serial_params, 0, sizeof(dcb_serial_params));
            dcb_serial_params.DCBlength = sizeof(dcb_serial_params);

            if (!GetCommState(serial_handle, &dcb_serial_params)) {
                std::cerr << "[ERR] Failed to get current serial port state: " << GetLastError() << std::endl;
                CloseHandle(serial_handle);
                serial_handle = INVALID_HANDLE_VALUE;
                return false;
            }

            dcb_serial_params.BaudRate = SERIAL_BAUD_RATE;
            dcb_serial_params.ByteSize = 8;
            dcb_serial_params.StopBits = ONESTOPBIT;
            dcb_serial_params.Parity = NOPARITY;

            if (!SetCommState(serial_handle, &dcb_serial_params)) {
                std::cerr << "[ERR] Failed to set serial port state: " << GetLastError() << std::endl;
                CloseHandle(serial_handle);
                serial_handle = INVALID_HANDLE_VALUE;
                return false;
            }            COMMTIMEOUTS timeouts;
            memset(&timeouts, 0, sizeof(timeouts));
            timeouts.WriteTotalTimeoutConstant = 100; // 100 ms write timeout (matching Python's timeout=0.1)
            timeouts.WriteTotalTimeoutMultiplier = 0;

            if (!SetCommTimeouts(serial_handle, &timeouts)) {
                std::cerr << "[ERR] Failed to set serial port timeouts: " << GetLastError() << std::endl;
                CloseHandle(serial_handle);
                serial_handle = INVALID_HANDLE_VALUE;
                return false;
            }
            
            // Give Arduino time to reset if DTR is enabled by serial connection (matching Python's time.sleep(2))
            Sleep(2000);
            
            std::cout << "[SERIAL] Successfully connected to Arduino on " << SERIAL_PORT << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERR] Serial initialization error: " << e.what() << std::endl;
            if (serial_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(serial_handle);
                serial_handle = INVALID_HANDLE_VALUE;
            }
            return false;
        }
    }

    void cleanup_serial() {
        if (serial_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(serial_handle);
            serial_handle = INVALID_HANDLE_VALUE;
            std::cout << "[SERIAL] Serial port " << SERIAL_PORT << " closed." << std::endl;
        }
    }
};

// Global pointer for signal handling
ForzaTelemetryProcessor* g_processor = nullptr;

// Signal handler for Ctrl+C
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            std::cout << std::endl << "[STOP] Shutting down telemetry bridge..." << std::endl;
            if (g_processor) {
                g_processor->stop();
            }
            return TRUE;
        default:
            return FALSE;
    }
}

int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << "🏎️  FORZA HORIZON → MOTION PLATFORM BRIDGE" << std::endl;
    std::cout << "============================================================" << std::endl;

    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        std::cerr << "[WARN] Could not set control handler for Ctrl+C." << std::endl;
    }

    try {
        ForzaTelemetryProcessor processor;
        g_processor = &processor;
        processor.run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Failed to start: " << e.what() << std::endl;
        std::cerr << "[FIX] Make sure port " << FORZA_UDP_PORT << " is available and try again" << std::endl;
        return 1;
    }
    
    g_processor = nullptr;
    return 0;
}
