// forza_to_arduino_gui.cpp
/*
Forza Horizon Telemetry Integration with GUI - Arduino Bridge

This application captures UDP telemetry data from Forza Horizon 4/5, calculates G-forces,
and forwards them to an Arduino (e.g., Mega 2560) via USB Serial with a Windows GUI interface.

Features:
- Real-time telemetry display
- Configurable network settings (IP, Port)
- Configurable Arduino settings (COM Port, Baud Rate)
- Adjustable update rate
- Start/Stop controls
- Status indicators
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
#include <cstring>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>

// Window controls IDs
#define ID_START_STOP_BTN       1001
#define ID_IP_EDIT             1002
#define ID_PORT_EDIT           1003
#define ID_SERIAL_PORT_EDIT    1004
#define ID_BAUD_RATE_EDIT      1005
#define ID_PULL_RATE_EDIT      1006
#define ID_STATUS_TEXT         1007
#define ID_SPEED_TEXT          1008
#define ID_RPM_TEXT            1009
#define ID_GFORCE_LONG_TEXT    1010
#define ID_GFORCE_LAT_TEXT     1011
#define ID_GFORCE_VERT_TEXT    1012
#define ID_ACTIVITY_TEXT       1013
#define ID_PACKETS_TEXT        1014

// Timer ID
#define ID_UPDATE_TIMER        2001

// Default configuration values
struct Config {
    std::string udp_ip = "127.0.0.1";
    int udp_port = 12345;
    std::string serial_port = "COM6";
    int baud_rate = 115200;
    double pull_rate = 1.0;
};

// Telemetry structures (same as before)
const int FORZA_NUM_FLOATS = 77;
const int FORZA_PACKET_SIZE_BYTES = FORZA_NUM_FLOATS * sizeof(float);

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

struct UIState {
    bool is_running = false;
    bool is_connected = false;
    int packet_count = 0;
    std::string last_error = "";
    TelemetryData current_telemetry = {};
    GForces current_gforces = {};
    bool arduino_success = false;
    bool is_active = false;
};

class ForzaTelemetryGUI {
public:    ForzaTelemetryGUI(HINSTANCE hInstance) : hInst(hInstance), sock(INVALID_SOCKET), 
                                            serial_handle(INVALID_HANDLE_VALUE), wsa_initialized(false),
                                            telemetry_thread(NULL) {
        config = {}; // Initialize with defaults
        ui_state = {};
        InitializeCriticalSection(&ui_mutex);
    }

    ~ForzaTelemetryGUI() {
        stop_telemetry();
        cleanup_serial();
        cleanup_udp_socket();
        cleanup_winsock();
        DeleteCriticalSection(&ui_mutex);
    }bool initialize() {
        // Initialize common controls using InitCommonControls instead
        InitCommonControls();

        return create_window();
    }

    void run() {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

private:    HINSTANCE hInst;
    HWND hWnd;
    Config config;
    UIState ui_state;
    CRITICAL_SECTION ui_mutex;
    
    // Network and serial
    SOCKET sock;
    HANDLE serial_handle;
    WSADATA wsa_data;
    bool wsa_initialized;
    std::atomic<bool> running{false};
    HANDLE telemetry_thread;

    // Window controls
    HWND hStartStopBtn, hIpEdit, hPortEdit, hSerialPortEdit, hBaudRateEdit, hPullRateEdit;
    HWND hStatusText, hSpeedText, hRpmText, hGforceLongText, hGforceLatText, hGforceVertText;
    HWND hActivityText, hPacketsText;

    bool create_window() {
        const char* className = "ForzaTelemetryGUI";
        
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInst;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

        if (!RegisterClassEx(&wc)) {
            return false;
        }

        hWnd = CreateWindowEx(
            0,
            className,
            "🏎️ Forza Horizon → Arduino Telemetry Bridge",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 650, 500,
            NULL, NULL, hInst, this
        );

        if (!hWnd) {
            return false;
        }

        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
        return true;
    }

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        ForzaTelemetryGUI* pThis = nullptr;

        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (ForzaTelemetryGUI*)pCreate->lpCreateParams;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
            pThis->hWnd = hWnd;
        } else {
            pThis = (ForzaTelemetryGUI*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        }

        if (pThis) {
            return pThis->handle_message(uMsg, wParam, lParam);
        }

        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    LRESULT handle_message(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_CREATE:
                create_controls();
                SetTimer(hWnd, ID_UPDATE_TIMER, 100, NULL); // Update UI every 100ms
                return 0;

            case WM_COMMAND:
                handle_command(LOWORD(wParam));
                return 0;

            case WM_TIMER:
                if (wParam == ID_UPDATE_TIMER) {
                    update_ui();
                }
                return 0;

            case WM_DESTROY:
                stop_telemetry();
                KillTimer(hWnd, ID_UPDATE_TIMER);
                PostQuitMessage(0);
                return 0;
        }

        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    void create_controls() {
        int y = 20;
        int x_label = 20;
        int x_input = 150;
        int input_width = 100;
        int line_height = 30;

        // Configuration section
        CreateWindow("STATIC", "Configuration:", WS_VISIBLE | WS_CHILD,
                    x_label, y, 200, 20, hWnd, NULL, hInst, NULL);
        y += 25;

        CreateWindow("STATIC", "UDP IP:", WS_VISIBLE | WS_CHILD,
                    x_label, y, 120, 20, hWnd, NULL, hInst, NULL);
        hIpEdit = CreateWindow("EDIT", config.udp_ip.c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER,
                              x_input, y, input_width, 22, hWnd, (HMENU)ID_IP_EDIT, hInst, NULL);
        y += line_height;

        CreateWindow("STATIC", "UDP Port:", WS_VISIBLE | WS_CHILD,
                    x_label, y, 120, 20, hWnd, NULL, hInst, NULL);
        hPortEdit = CreateWindow("EDIT", std::to_string(config.udp_port).c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER,
                                x_input, y, input_width, 22, hWnd, (HMENU)ID_PORT_EDIT, hInst, NULL);
        y += line_height;

        CreateWindow("STATIC", "Serial Port:", WS_VISIBLE | WS_CHILD,
                    x_label, y, 120, 20, hWnd, NULL, hInst, NULL);
        hSerialPortEdit = CreateWindow("EDIT", config.serial_port.c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER,
                                     x_input, y, input_width, 22, hWnd, (HMENU)ID_SERIAL_PORT_EDIT, hInst, NULL);
        y += line_height;

        CreateWindow("STATIC", "Baud Rate:", WS_VISIBLE | WS_CHILD,
                    x_label, y, 120, 20, hWnd, NULL, hInst, NULL);
        hBaudRateEdit = CreateWindow("EDIT", std::to_string(config.baud_rate).c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER,
                                   x_input, y, input_width, 22, hWnd, (HMENU)ID_BAUD_RATE_EDIT, hInst, NULL);
        y += line_height;

        CreateWindow("STATIC", "Update Rate (s):", WS_VISIBLE | WS_CHILD,
                    x_label, y, 120, 20, hWnd, NULL, hInst, NULL);
        hPullRateEdit = CreateWindow("EDIT", std::to_string(config.pull_rate).c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER,
                                   x_input, y, input_width, 22, hWnd, (HMENU)ID_PULL_RATE_EDIT, hInst, NULL);
        y += line_height;

        // Start/Stop button
        y += 10;
        hStartStopBtn = CreateWindow("BUTTON", "Start Telemetry", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                   x_label, y, 150, 35, hWnd, (HMENU)ID_START_STOP_BTN, hInst, NULL);

        // Status section
        y += 50;
        CreateWindow("STATIC", "Status:", WS_VISIBLE | WS_CHILD,
                    x_label, y, 200, 20, hWnd, NULL, hInst, NULL);
        y += 25;

        hStatusText = CreateWindow("STATIC", "Stopped", WS_VISIBLE | WS_CHILD,
                                 x_label, y, 300, 20, hWnd, (HMENU)ID_STATUS_TEXT, hInst, NULL);
        y += line_height;

        hActivityText = CreateWindow("STATIC", "Activity: IDLE", WS_VISIBLE | WS_CHILD,
                                   x_label, y, 200, 20, hWnd, (HMENU)ID_ACTIVITY_TEXT, hInst, NULL);
        y += line_height;

        hPacketsText = CreateWindow("STATIC", "Packets: 0", WS_VISIBLE | WS_CHILD,
                                  x_label, y, 200, 20, hWnd, (HMENU)ID_PACKETS_TEXT, hInst, NULL);

        // Telemetry data section
        y = 20;
        int x_right = 350;

        CreateWindow("STATIC", "Telemetry Data:", WS_VISIBLE | WS_CHILD,
                    x_right, y, 200, 20, hWnd, NULL, hInst, NULL);
        y += 25;

        hSpeedText = CreateWindow("STATIC", "Speed: 0.0 mph (0.0 km/h)", WS_VISIBLE | WS_CHILD,
                                x_right, y, 250, 20, hWnd, (HMENU)ID_SPEED_TEXT, hInst, NULL);
        y += line_height;

        hRpmText = CreateWindow("STATIC", "RPM: 0", WS_VISIBLE | WS_CHILD,
                              x_right, y, 200, 20, hWnd, (HMENU)ID_RPM_TEXT, hInst, NULL);
        y += line_height;

        CreateWindow("STATIC", "G-Forces:", WS_VISIBLE | WS_CHILD,
                    x_right, y, 200, 20, hWnd, NULL, hInst, NULL);
        y += 25;

        hGforceLongText = CreateWindow("STATIC", "Longitudinal: +0.000G", WS_VISIBLE | WS_CHILD,
                                     x_right, y, 200, 20, hWnd, (HMENU)ID_GFORCE_LONG_TEXT, hInst, NULL);
        y += line_height;

        hGforceLatText = CreateWindow("STATIC", "Lateral: +0.000G", WS_VISIBLE | WS_CHILD,
                                    x_right, y, 200, 20, hWnd, (HMENU)ID_GFORCE_LAT_TEXT, hInst, NULL);
        y += line_height;

        hGforceVertText = CreateWindow("STATIC", "Vertical: +1.000G", WS_VISIBLE | WS_CHILD,
                                     x_right, y, 200, 20, hWnd, (HMENU)ID_GFORCE_VERT_TEXT, hInst, NULL);
    }

    void handle_command(int controlId) {
        switch (controlId) {
            case ID_START_STOP_BTN:
                if (ui_state.is_running) {
                    stop_telemetry();
                } else {
                    start_telemetry();
                }
                break;
        }
    }

    void start_telemetry() {
        // Read configuration from UI
        read_config_from_ui();

        // Initialize networking and serial
        if (!initialize_winsock()) {
            show_error("Failed to initialize Winsock");
            return;
        }

        if (!initialize_udp_socket()) {
            show_error("Failed to initialize UDP socket");
            cleanup_winsock();
            return;
        }

        if (!initialize_serial()) {
            show_error("Failed to initialize serial connection");
            cleanup_udp_socket();
            cleanup_winsock();
            return;
        }        // Start telemetry thread
        running = true;
        telemetry_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)telemetry_worker_wrapper, this, 0, NULL);

        // Update UI
        EnterCriticalSection(&ui_mutex);
        ui_state.is_running = true;
        SetWindowTextA(hStartStopBtn, "Stop Telemetry");
        LeaveCriticalSection(&ui_mutex);
        
        // Disable configuration controls
        EnableWindow(hIpEdit, FALSE);
        EnableWindow(hPortEdit, FALSE);
        EnableWindow(hSerialPortEdit, FALSE);
        EnableWindow(hBaudRateEdit, FALSE);
    }

    void stop_telemetry() {
        running = false;
        
        if (telemetry_thread) {
            if (telemetry_thread->joinable()) {
                telemetry_thread->join();
            }
            delete telemetry_thread;
            telemetry_thread = nullptr;
        }

        cleanup_serial();
        cleanup_udp_socket();
        cleanup_winsock();

        // Update UI
        std::lock_guard<std::mutex> lock(ui_mutex);
        ui_state.is_running = false;
        ui_state.is_connected = false;
        ui_state.packet_count = 0;
        SetWindowText(hStartStopBtn, "Start Telemetry");
        
        // Enable configuration controls
        EnableWindow(hIpEdit, TRUE);
        EnableWindow(hPortEdit, TRUE);
        EnableWindow(hSerialPortEdit, TRUE);
        EnableWindow(hBaudRateEdit, TRUE);
    }

    void read_config_from_ui() {
        char buffer[256];
        
        GetWindowText(hIpEdit, buffer, sizeof(buffer));
        config.udp_ip = buffer;
        
        GetWindowText(hPortEdit, buffer, sizeof(buffer));
        config.udp_port = atoi(buffer);
        
        GetWindowText(hSerialPortEdit, buffer, sizeof(buffer));
        config.serial_port = buffer;
        
        GetWindowText(hBaudRateEdit, buffer, sizeof(buffer));
        config.baud_rate = atoi(buffer);
        
        GetWindowText(hPullRateEdit, buffer, sizeof(buffer));
        config.pull_rate = atof(buffer);
    }

    void update_ui() {
        std::lock_guard<std::mutex> lock(ui_mutex);
        
        // Update status
        std::string status = ui_state.is_running ? 
            (ui_state.is_connected ? "🟢 Connected" : "⚡ Waiting for Forza...") : 
            "🔴 Stopped";
        SetWindowText(hStatusText, status.c_str());

        // Update activity
        std::string activity = ui_state.is_active ? "🏁 ACTIVE" : "⏸️ IDLE";
        SetWindowText(hActivityText, activity.c_str());

        // Update packet count
        std::string packets = "Packets: " + std::to_string(ui_state.packet_count);
        SetWindowText(hPacketsText, packets.c_str());

        if (ui_state.is_connected) {
            // Update telemetry data
            std::ostringstream speed_ss;
            speed_ss << "Speed: " << std::fixed << std::setprecision(1) 
                     << ui_state.current_telemetry.speed_mph << " mph ("
                     << (ui_state.current_telemetry.speed * 3.6) << " km/h)";
            SetWindowText(hSpeedText, speed_ss.str().c_str());

            std::string rpm = "RPM: " + std::to_string((int)ui_state.current_telemetry.current_engine_rpm);
            SetWindowText(hRpmText, rpm.c_str());

            // Update G-forces
            std::ostringstream long_ss, lat_ss, vert_ss;
            long_ss << "Longitudinal: " << std::showpos << std::fixed << std::setprecision(3) 
                    << ui_state.current_gforces.longitudinal << "G";
            SetWindowText(hGforceLongText, long_ss.str().c_str());

            lat_ss << "Lateral: " << std::showpos << std::fixed << std::setprecision(3) 
                   << ui_state.current_gforces.lateral << "G";
            SetWindowText(hGforceLatText, lat_ss.str().c_str());

            vert_ss << "Vertical: " << std::showpos << std::fixed << std::setprecision(3) 
                    << ui_state.current_gforces.vertical << "G";
            SetWindowText(hGforceVertText, vert_ss.str().c_str());
        }
    }

    void show_error(const std::string& message) {
        MessageBox(hWnd, message.c_str(), "Error", MB_OK | MB_ICONERROR);
    }

    // Telemetry processing methods (adapted from original code)
    void telemetry_worker() {
        int packet_count = 0;
        auto last_status_time = std::chrono::steady_clock::now();
        auto last_data_time = std::chrono::steady_clock::now();

        while (running) {
            try {
                char recv_buffer[1024];
                sockaddr_in sender_addr;
                int sender_addr_size = sizeof(sender_addr);
                
                int bytes_received = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0, 
                                            (SOCKADDR*)&sender_addr, &sender_addr_size);
                
                if (bytes_received == SOCKET_ERROR) {
                    int error = WSAGetLastError();
                    if (error == WSAETIMEDOUT) {
                        auto current_time = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_data_time);
                        if (elapsed.count() > 10) {
                            last_data_time = current_time;
                        }
                        continue;
                    } else {
                        continue;
                    }
                }
                
                packet_count++;
                last_data_time = std::chrono::steady_clock::now();
                
                // Parse telemetry data
                TelemetryData* telemetry = parse_telemetry_packet(recv_buffer, bytes_received);
                if (!telemetry) {
                    continue;
                }
                
                // Check if car is moving
                double speed_kmh = telemetry->speed * 3.6;
                double rpm = telemetry->current_engine_rpm;
                bool is_active = speed_kmh > 1.0 || rpm > 1000;
                
                GForces g_forces;
                bool success;
                
                if (is_active) {
                    g_forces = calculate_g_forces(*telemetry);
                    success = send_to_arduino(g_forces);
                } else {
                    auto now = std::chrono::system_clock::now();
                    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                    g_forces = {0.0, 0.0, 1.0, timestamp};
                    success = send_to_arduino(g_forces);
                }
                
                // Update UI state
                {
                    std::lock_guard<std::mutex> lock(ui_mutex);
                    ui_state.is_connected = true;
                    ui_state.packet_count = packet_count;
                    ui_state.current_telemetry = *telemetry;
                    ui_state.current_gforces = g_forces;
                    ui_state.arduino_success = success;
                    ui_state.is_active = is_active;
                }
                
            } catch (const std::exception& e) {
                continue;
            }
        }
    }

    TelemetryData* parse_telemetry_packet(const char* data, int data_len) {
        try {
            if (data_len < 308) {
                return nullptr;
            }

            const float* values = reinterpret_cast<const float*>(data);
            static TelemetryData telemetry;
            
            telemetry.current_engine_rpm = values[4];
            telemetry.accel_x = values[5];
            telemetry.accel_y = values[6];
            telemetry.accel_z = values[7];
            telemetry.velocity_x = values[8];
            telemetry.velocity_y = values[9];
            telemetry.velocity_z = values[10];
            
            telemetry.speed = std::sqrt(
                telemetry.velocity_x * telemetry.velocity_x +
                telemetry.velocity_y * telemetry.velocity_y +
                telemetry.velocity_z * telemetry.velocity_z
            );
            telemetry.speed_mph = telemetry.speed * 2.23694;

            return &telemetry;
            
        } catch (const std::exception& e) {
            return nullptr;
        }
    }

    GForces calculate_g_forces(const TelemetryData& telemetry) {
        const double G_FORCE = 9.81;
        
        double accel_x = telemetry.accel_x;
        double accel_y = telemetry.accel_y;
        double accel_z = telemetry.accel_z;
        
        double g_longitudinal = -accel_z / G_FORCE;
        double g_lateral = accel_x / G_FORCE;
        double g_vertical = (accel_y / G_FORCE) + 1.0;
        
        g_longitudinal = std::max(-3.0, std::min(3.0, g_longitudinal));
        g_lateral = std::max(-3.0, std::min(3.0, g_lateral));
        g_vertical = std::max(-1.0, std::min(4.0, g_vertical));
        
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
            return false;
        }
        
        try {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3)
                << g_forces.longitudinal << ","
                << g_forces.lateral << ","
                << g_forces.vertical << "\\n";
            
            std::string data_string = oss.str();
            
            DWORD bytes_written;
            if (!WriteFile(serial_handle, data_string.c_str(), static_cast<DWORD>(data_string.length()), &bytes_written, NULL)) {
                return false;
            }
            return bytes_written == data_string.length();
            
        } catch (const std::exception& e) {
            return false;
        }
    }

    double round_to_places(double value, int places) {
        double multiplier = std::pow(10.0, places);
        return std::round(value * multiplier) / multiplier;
    }

    // Network and Serial initialization methods (same as before but adapted)
    bool initialize_winsock() {
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
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
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            return false;
        }

        BOOL reuseAddr = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr));
        
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config.udp_port);
        server_addr.sin_addr.s_addr = inet_addr(config.udp_ip.c_str());

        if (bind(sock, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            return false;
        }

        DWORD timeout_ms = 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
        
        return true;
    }

    void cleanup_udp_socket() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
    }

    bool initialize_serial() {
        serial_handle = CreateFileA(config.serial_port.c_str(),
                                   GENERIC_READ | GENERIC_WRITE,
                                   0, NULL, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, NULL);

        if (serial_handle == INVALID_HANDLE_VALUE) {
            return false;
        }

        DCB dcb_serial_params;
        memset(&dcb_serial_params, 0, sizeof(dcb_serial_params));
        dcb_serial_params.DCBlength = sizeof(dcb_serial_params);

        if (!GetCommState(serial_handle, &dcb_serial_params)) {
            CloseHandle(serial_handle);
            serial_handle = INVALID_HANDLE_VALUE;
            return false;
        }

        dcb_serial_params.BaudRate = config.baud_rate;
        dcb_serial_params.ByteSize = 8;
        dcb_serial_params.StopBits = ONESTOPBIT;
        dcb_serial_params.Parity = NOPARITY;

        if (!SetCommState(serial_handle, &dcb_serial_params)) {
            CloseHandle(serial_handle);
            serial_handle = INVALID_HANDLE_VALUE;
            return false;
        }

        COMMTIMEOUTS timeouts;
        memset(&timeouts, 0, sizeof(timeouts));
        timeouts.WriteTotalTimeoutConstant = 100;
        timeouts.WriteTotalTimeoutMultiplier = 0;

        if (!SetCommTimeouts(serial_handle, &timeouts)) {
            CloseHandle(serial_handle);
            serial_handle = INVALID_HANDLE_VALUE;
            return false;
        }
        
        Sleep(2000);
        return true;
    }

    void cleanup_serial() {
        if (serial_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(serial_handle);
            serial_handle = INVALID_HANDLE_VALUE;
        }
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    ForzaTelemetryGUI app(hInstance);
    
    if (!app.initialize()) {
        MessageBox(NULL, "Failed to initialize application", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    app.run();
    return 0;
}
