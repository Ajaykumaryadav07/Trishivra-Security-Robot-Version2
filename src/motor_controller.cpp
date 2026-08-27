#include "robot_modules.h"
#include "robot_state.h"
#include "config.h"
#include "robot_protocol.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <algorithm>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#endif
#include <atomic>
#include <mutex>

// ============================================================
// SerialPort Class (Internal Helper)
// ============================================================

class SerialPort {
public:
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int fd = -1;
#endif
    std::mutex write_mutex;
    
    ~SerialPort() {
        close();
    }
    
    bool open(const std::string& port, int baud) {
#ifdef _WIN32
        std::string port_name = "\\\\.\\" + port;
        handle = CreateFileA(port_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            std::cerr << "[SERIAL] Failed to open " << port << " on Windows" << std::endl;
            return false;
        }

        DCB dcb = {0};
        dcb.DCBlength = sizeof(DCB);
        if (!GetCommState(handle, &dcb)) {
            std::cerr << "[SERIAL] Failed to get comm state" << std::endl;
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
            return false;
        }

        dcb.BaudRate = baud;
        dcb.ByteSize = 8;
        dcb.Parity   = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;

        if (!SetCommState(handle, &dcb)) {
            std::cerr << "[SERIAL] Failed to set comm state" << std::endl;
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
            return false;
        }

        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 10;
        SetCommTimeouts(handle, &timeouts);

        return true;
#else
        fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0) {
            std::cerr << "[SERIAL] Failed to open " << port << std::endl;
            return false;
        }
        
        struct termios tty;
        if (tcgetattr(fd, &tty) != 0) {
            std::cerr << "[SERIAL] Failed to get attributes" << std::endl;
            return false;
        }
        
        // Set baud rate
        speed_t baud_rate;
        switch (baud) {
            case 9600: baud_rate = B9600; break;
            case 19200: baud_rate = B19200; break;
            case 38400: baud_rate = B38400; break;
            case 57600: baud_rate = B57600; break;
            case 115200: baud_rate = B115200; break;
            default: baud_rate = B115200; break;
        }
        
        cfsetospeed(&tty, baud_rate);
        cfsetispeed(&tty, baud_rate);
        
        // Configure
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_iflag &= ~IGNBRK;
        tty.c_lflag = 0;
        tty.c_oflag = 0;
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 5;
        
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~(PARENB | PARODD);
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;
        
        if (tcsetattr(fd, TCSANOW, &tty) != 0) {
            std::cerr << "[SERIAL] Failed to set attributes" << std::endl;
            return false;
        }
        
        return true;
#endif
    }
    
    void close() {
#ifdef _WIN32
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
#else
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
#endif
    }
    
    bool write(const char* data, size_t len) {
#ifdef _WIN32
        if (handle == INVALID_HANDLE_VALUE) return false;
        std::lock_guard<std::mutex> lock(write_mutex);
        DWORD bytes_written = 0;
        return WriteFile(handle, data, static_cast<DWORD>(len), &bytes_written, nullptr) && bytes_written == len;
#else
        if (fd < 0) return false;
        std::lock_guard<std::mutex> lock(write_mutex);
        return ::write(fd, data, len) == static_cast<ssize_t>(len);
#endif
    }
    
    int read(char* buffer, size_t max_len) {
#ifdef _WIN32
        if (handle == INVALID_HANDLE_VALUE) return -1;
        DWORD bytes_read = 0;
        if (!ReadFile(handle, buffer, static_cast<DWORD>(max_len), &bytes_read, nullptr)) {
            return -1;
        }
        return static_cast<int>(bytes_read);
#else
        if (fd < 0) return -1;
        return ::read(fd, buffer, max_len);
#endif
    }
    
    int available() {
#ifdef _WIN32
        if (handle == INVALID_HANDLE_VALUE) return 0;
        COMSTAT status;
        DWORD errors;
        if (ClearCommError(handle, &errors, &status)) {
            return static_cast<int>(status.cbInQue);
        }
        return 0;
#else
        if (fd < 0) return 0;
        int count = 0;
        ioctl(fd, FIONREAD, &count);
        return count;
#endif
    }
    
    bool is_open() const {
#ifdef _WIN32
        return handle != INVALID_HANDLE_VALUE;
#else
        return fd >= 0;
#endif
    }
};

// ============================================================
// MotorController Implementation
// ============================================================

MotorController::MotorController() 
    : serial(nullptr), 
      connected(false), 
      avoiding(false),
      last_message("") {
    std::cout << "[MOTOR] Motor controller created" << std::endl;
}

MotorController::~MotorController() {
    stop();
    if (serial) {
        delete static_cast<SerialPort*>(serial);
        serial = nullptr;
    }
    std::cout << "[MOTOR] Motor controller destroyed" << std::endl;
}

// ============================================================
// Initialize Serial Connection
// ============================================================

bool MotorController::init(const std::string& port, int baud) {
    try {
        serial = new SerialPort();
        if (!static_cast<SerialPort*>(serial)->open(port, baud)) {
            delete static_cast<SerialPort*>(serial);
            serial = nullptr;
            std::cerr << "[MOTOR] Failed to open " << port << std::endl;
            return false;
        }
        
        connected = true;
        std::cout << "[MOTOR] Connected to " << port << " @ " << baud << " baud" << std::endl;
        
        // Wait for ESP32 to reset
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Start background reader thread
        std::thread([this]() { read_thread_func(); }).detach();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MOTOR] Init error: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================
// Set Motor Speeds
// ============================================================

void MotorController::set_speed(float left, float right) {
    if (!connected || !serial) {
        return;
    }
    
    // Clamp values
    left = std::max(-1.0f, std::min(1.0f, left));
    right = std::max(-1.0f, std::min(1.0f, right));
    
    // Convert to motor commands
    char cmd = RobotProtocol::CMD_STOP;
    
    // If both motors are stopped
    if (std::abs(left) < 0.05f && std::abs(right) < 0.05f) {
        cmd = RobotProtocol::CMD_STOP;
    }
    // Both forward
    else if (left > 0.1f && right > 0.1f) {
        cmd = RobotProtocol::CMD_FORWARD;
    }
    // Both backward
    else if (left < -0.1f && right < -0.1f) {
        cmd = RobotProtocol::CMD_BACKWARD;
    }
    // Turning left (left slower than right)
    else if (left < right) {
        cmd = RobotProtocol::CMD_TURN_LEFT;
    }
    // Turning right (right slower than left)
    else if (right < left) {
        cmd = RobotProtocol::CMD_TURN_RIGHT;
    }
    
    send_command(cmd);
}

// ============================================================
// Stop Motors
// ============================================================

void MotorController::stop() {
    send_command(RobotProtocol::CMD_STOP);
}

// ============================================================
// Send Command to ESP32
// ============================================================

void MotorController::send_command(char cmd) {
    if (!connected || !serial) {
        return;
    }
    
    try {
        static_cast<SerialPort*>(serial)->write(&cmd, 1);
        // std::cout << "[MOTOR] Command: " << cmd << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[MOTOR] Send error: " << e.what() << std::endl;
    }
}

// ============================================================
// Set Steering Angle
// ============================================================

void MotorController::set_steering_angle(int angle) {
    if (!connected || !serial) {
        return;
    }
    
    // Clamp angle
    angle = std::max(0, std::min(180, angle));
    
    std::string cmd = "C" + std::to_string(angle) + "\n";
    
    try {
        static_cast<SerialPort*>(serial)->write(cmd.c_str(), cmd.length());
        // std::cout << "[MOTOR] Steering: " << angle << "°" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[MOTOR] Steering error: " << e.what() << std::endl;
    }
}

// ============================================================
// Get Status
// ============================================================

bool MotorController::is_avoiding() const {
    return avoiding;
}

std::string MotorController::get_last_message() const {
    return last_message;
}

// ============================================================
// Background Reader Thread
// ============================================================

void MotorController::read_thread_func() {
    char buffer[256];
    SerialPort* ser = static_cast<SerialPort*>(serial);
    
    while (connected && ser && ser->is_open()) {
        try {
            int n = ser->read(buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                std::string response(buffer);
                
                // Process complete lines
                size_t pos = 0;
                while ((pos = response.find('\n')) != std::string::npos) {
                    std::string line = response.substr(0, pos);
                    response.erase(0, pos + 1);
                    
                    if (!line.empty()) {
                        process_response(line);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[MOTOR] Read error: " << e.what() << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ============================================================
// Process ESP32 Response
// ============================================================

void MotorController::process_response(const std::string& response) {
    last_message = response;
    
    // Check for obstacle detection
    if (response.find(RobotProtocol::RESP_OBSTACLE_DETECTED) != std::string::npos) {
        avoiding = true;
        g_robot_state.obstacle_detected = true;
        g_robot_state.is_avoiding = true;
        set_state(RobotStateCode::OBSTACLE_AVOID);
        std::cout << "[ESP32] 🚧 Obstacle detected!" << std::endl;
    }
    // Check for avoidance complete
    else if (response.find(RobotProtocol::RESP_AVOIDANCE_COMPLETE) != std::string::npos) {
        avoiding = false;
        g_robot_state.obstacle_detected = false;
        g_robot_state.is_avoiding = false;
        if (get_state() == RobotStateCode::OBSTACLE_AVOID) {
            set_state(RobotStateCode::PATROL);
        }
        std::cout << "[ESP32] ✅ Avoidance complete" << std::endl;
    }
    // Check for emergency
    else if (response.find(RobotProtocol::RESP_EMERGENCY) != std::string::npos) {
        g_robot_state.current_state = RobotStateCode::EMERGENCY_STOP;
        std::cout << "[ESP32] 🚨 EMERGENCY!" << std::endl;
    }
    // Check for turning messages
    else if (response.find(RobotProtocol::RESP_TURNING_LEFT) != std::string::npos) {
        std::cout << "[ESP32] 🔄 Turning Left" << std::endl;
    }
    else if (response.find(RobotProtocol::RESP_TURNING_RIGHT) != std::string::npos) {
        std::cout << "[ESP32] 🔄 Turning Right" << std::endl;
    }
    // Command confirmations
    else if (response.find(RobotProtocol::RESP_CMD_FORWARD) != std::string::npos) {
        // std::cout << "[ESP32] Forward confirmed" << std::endl;
    }
    else if (response.find(RobotProtocol::RESP_CMD_STOP) != std::string::npos) {
        // std::cout << "[ESP32] Stop confirmed" << std::endl;
    }
    // Steering angle set
    else if (response.find(RobotProtocol::RESP_ANGLE_SET) != std::string::npos) {
        // std::cout << "[ESP32] Steering set: " << response << std::endl;
    }
    // Unknown message
    else if (!response.empty()) {
        // Only log if not empty and not common
        if (response.length() > 2) {
            std::cout << "[ESP32] " << response << std::endl;
        }
    }
}

// ============================================================
// Additional Commands (Virtual Obstacles for Testing)
// ============================================================

void MotorController::send_virtual_obstacle_left() {
    send_command('O');
    std::cout << "[MOTOR] Virtual obstacle LEFT sent" << std::endl;
}

void MotorController::send_virtual_obstacle_right() {
    send_command('P');
    std::cout << "[MOTOR] Virtual obstacle RIGHT sent" << std::endl;
}

// ============================================================
// Get Status Command (Debug)
// ============================================================

void MotorController::request_status() {
    send_command(RobotProtocol::CMD_GET_STATUS);
}

// ============================================================
// Flush Serial Buffer
// ============================================================

void MotorController::flush_buffer() {
    if (!connected || !serial) {
        return;
    }
    
    try {
        SerialPort* ser = static_cast<SerialPort*>(serial);
        while (ser->available() > 0) {
            char buffer[64];
            ser->read(buffer, sizeof(buffer));
        }
    } catch (...) {
        // Ignore errors
    }
}