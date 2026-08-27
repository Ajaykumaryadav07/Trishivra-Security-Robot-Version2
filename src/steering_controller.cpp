#include "robot_modules.h"
#include "robot_state.h"
#include "config.h"
#include "robot_protocol.h"
#include <iostream>
#include <cmath>
#include <mutex>
#include <chrono>

// ============================================================
// SteeringController Implementation
// ============================================================

class SteeringController {
private:
    // Serial port reference
    void* serial_port;
    bool connected;
    bool initialized;
    
    // Current steering state
    int current_angle;
    int target_angle;
    float current_angle_degrees;
    
    // PID Controller for steering
    float kp;
    float ki;
    float kd;
    float integral;
    float previous_error;
    std::mutex pid_mutex;
    
    // Calibration
    int pot_min;
    int pot_max;
    int neutral_angle;
    int max_delta;
    
    // Timing
    unsigned long last_update_time;
    float update_interval_ms;
    
public:
    SteeringController();
    ~SteeringController();
    
    bool init(void* serial, int port = -1);
    void set_angle(int angle);
    void set_angle_degrees(float degrees);
    int get_angle() const { return current_angle; }
    bool is_initialized() const { return initialized; }
    
    // PID tuning
    void set_pid(float p, float i, float d);
    void calibrate_pot(int min_val, int max_val);
    
    // Update loop (call at fixed rate)
    void update(float current_pot_value);
    void reset_pid();
    
    // Debug
    void print_status();
};

// ============================================================
// Constructor
// ============================================================

SteeringController::SteeringController()
    : serial_port(nullptr),
      connected(false),
      initialized(false),
      current_angle(90),
      target_angle(90),
      current_angle_degrees(0.0f),
      kp(KP),
      ki(KI),
      kd(KD),
      integral(0.0f),
      previous_error(0.0f),
      pot_min(POT_MIN),
      pot_max(POT_MAX),
      neutral_angle(STRAIGHT_ANGLE),
      max_delta(STEER_MAX_DELTA),
      last_update_time(0),
      update_interval_ms(1000.0f / STEER_UPDATE_HZ) {
    
    std::cout << "[STEERING] Controller created" << std::endl;
    std::cout << "[STEERING] PID: Kp=" << kp << ", Ki=" << ki << ", Kd=" << kd << std::endl;
    std::cout << "[STEERING] Neutral=" << neutral_angle << "°, MaxDelta=" << max_delta << "°" << std::endl;
}

// ============================================================
// Destructor
// ============================================================

SteeringController::~SteeringController() {
    std::cout << "[STEERING] Controller destroyed" << std::endl;
}

// ============================================================
// Initialize Steering Controller
// ============================================================

bool SteeringController::init(void* serial, int port) {
    if (!serial) {
        std::cerr << "[STEERING] Invalid serial port" << std::endl;
        return false;
    }
    
    serial_port = serial;
    connected = true;
    initialized = true;
    
    // Reset PID
    reset_pid();
    
    // Set neutral angle
    set_angle(neutral_angle);
    
    std::cout << "[STEERING] Initialized" << std::endl;
    return true;
}

// ============================================================
// Set Steering Angle (0-180)
// ============================================================

void SteeringController::set_angle(int angle) {
    if (!connected || !initialized) {
        return;
    }
    
    // Clamp angle to valid range
    angle = std::max(0, std::min(180, angle));
    
    // Apply maximum delta limit (prevents sudden jerks)
    int delta = angle - current_angle;
    if (std::abs(delta) > max_delta) {
        angle = current_angle + (delta > 0 ? max_delta : -max_delta);
    }
    
    target_angle = angle;
    
    // Send command to ESP32
    std::string cmd = "C" + std::to_string(angle) + "\n";
    
    try {
        // Prefer using MotorController interface if available
        MotorController* mc = static_cast<MotorController*>(serial_port);
        if (mc) {
            mc->set_steering_angle(angle);
        }
    } catch (const std::exception& e) {
        std::cerr << "[STEERING] Send error: " << e.what() << std::endl;
    }
    
    current_angle = angle;
    current_angle_degrees = angle;
}

// ============================================================
// Set Steering Angle in Degrees (0-180)
// ============================================================

void SteeringController::set_angle_degrees(float degrees) {
    int angle = static_cast<int>(std::round(degrees));
    set_angle(angle);
}

// ============================================================
// PID Update Loop
// ============================================================

void SteeringController::update(float current_pot_value) {
    if (!initialized || !connected) {
        return;
    }
    
    // Check if update interval has passed
    unsigned long now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    
    if (now - last_update_time < update_interval_ms) {
        return;
    }
    last_update_time = now;
    
    // Convert potentiometer value to angle
    int pot_value = static_cast<int>(current_pot_value);
    pot_value = std::max(pot_min, std::min(pot_max, pot_value));
    
    // Map potentiometer to angle (0-180)
    int current_feedback = (pot_value - pot_min) * 180 / (pot_max - pot_min);
    current_feedback = std::max(0, std::min(180, current_feedback));
    
    // Calculate error
    int error = target_angle - current_feedback;
    
    // Check if within threshold
    if (std::abs(error) <= 2) {
        // Stop steering motor
        if (serial_port) {
            try {
                MotorController* mc = static_cast<MotorController*>(serial_port);
                if (mc && mc->is_connected()) {
                    // Optionally use MotorController API to send a neutral/stop command
                    // mc->set_steering_angle(current_angle);
                }
            } catch (...) {}
        }
        integral = 0.0f;
        return;
    }
    
    // PID calculation
    std::lock_guard<std::mutex> lock(pid_mutex);
    
    integral += error;
    integral = std::max(-1000.0f, std::min(1000.0f, integral));
    
    float derivative = error - previous_error;
    previous_error = error;
    
    float output = (kp * error) + (ki * integral) + (kd * derivative);
    int speed = static_cast<int>(std::abs(output));
    speed = std::max(0, std::min(255, speed));
    
    // Determine direction
    // Note: This would send direction + speed to ESP32
    // For now, we just log the output
    if (std::abs(output) > 5) {
        // std::cout << "[STEERING] PID: error=" << error 
        //           << ", output=" << output 
        //           << ", speed=" << speed << std::endl;
    }
}

// ============================================================
// Reset PID Controller
// ============================================================

void SteeringController::reset_pid() {
    std::lock_guard<std::mutex> lock(pid_mutex);
    integral = 0.0f;
    previous_error = 0.0f;
}

// ============================================================
// Set PID Parameters
// ============================================================

void SteeringController::set_pid(float p, float i, float d) {
    std::lock_guard<std::mutex> lock(pid_mutex);
    kp = p;
    ki = i;
    kd = d;
    std::cout << "[STEERING] PID updated: Kp=" << kp 
              << ", Ki=" << ki << ", Kd=" << kd << std::endl;
}

// ============================================================
// Calibrate Potentiometer
// ============================================================

void SteeringController::calibrate_pot(int min_val, int max_val) {
    pot_min = min_val;
    pot_max = max_val;
    std::cout << "[STEERING] Pot calibration: " << pot_min 
              << " - " << pot_max << std::endl;
}

// ============================================================
// Print Status
// ============================================================

void SteeringController::print_status() {
    std::cout << "[STEERING] Status:" << std::endl;
    std::cout << "  Target: " << target_angle << "°" << std::endl;
    std::cout << "  Current: " << current_angle << "°" << std::endl;
    std::cout << "  Neutral: " << neutral_angle << "°" << std::endl;
    std::cout << "  MaxDelta: " << max_delta << "°" << std::endl;
    std::cout << "  PID: Kp=" << kp << ", Ki=" << ki << ", Kd=" << kd << std::endl;
    std::cout << "  Integral: " << integral << std::endl;
    std::cout << "  Previous Error: " << previous_error << std::endl;
}

// ============================================================
// Global Function Wrapper (for use in main.cpp)
// ============================================================

// Global instance
static SteeringController g_steering_controller;

bool steering_init(void* serial) {
    return g_steering_controller.init(serial);
}

void steering_set_angle(int angle) {
    g_steering_controller.set_angle(angle);
}

void steering_set_angle_degrees(float degrees) {
    g_steering_controller.set_angle_degrees(degrees);
}

int steering_get_angle() {
    return g_steering_controller.get_angle();
}

void steering_update(float pot_value) {
    g_steering_controller.update(pot_value);
}

void steering_reset_pid() {
    g_steering_controller.reset_pid();
}

void steering_set_pid(float p, float i, float d) {
    g_steering_controller.set_pid(p, i, d);
}

void steering_calibrate_pot(int min_val, int max_val) {
    g_steering_controller.calibrate_pot(min_val, max_val);
}

void steering_print_status() {
    g_steering_controller.print_status();
}

bool steering_is_initialized() {
    return g_steering_controller.is_initialized();
}