#include "robot_state.h"
#include "config.h"
#include "robot_protocol.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>

// ============================================================
// Global State Instance
// ============================================================

RobotStateData g_robot_state;

// ============================================================
// State Name Helper
// ============================================================

const char* get_state_name(RobotStateCode state) {
    switch (state) {
        case RobotStateCode::IDLE:           return "IDLE";
        case RobotStateCode::PATROL:         return "PATROL";
        case RobotStateCode::MAPPING:        return "MAPPING";
        case RobotStateCode::THREAT_DETECTED: return "THREAT_DETECTED";
        case RobotStateCode::EMERGENCY_STOP: return "EMERGENCY_STOP";
        case RobotStateCode::RETURN_HOME:    return "RETURN_HOME";
        case RobotStateCode::OBSTACLE_AVOID: return "OBSTACLE_AVOID";
        case RobotStateCode::PAUSED:         return "PAUSED";
        case RobotStateCode::SHUTDOWN:       return "SHUTDOWN";
        default:                             return "UNKNOWN";
    }
}

const char* get_state_name() {
    return get_state_name(g_robot_state.current_state);
}

// ============================================================
// Set State
// ============================================================

void set_state(RobotStateCode new_state) {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    
    // Don't allow transition to same state (except for patrol)
    if (g_robot_state.current_state == new_state && new_state != RobotStateCode::PATROL) {
        return;
    }
    
    g_robot_state.previous_state = g_robot_state.current_state;
    g_robot_state.current_state = new_state;
    g_robot_state.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    
    std::cout << "[STATE] " << get_state_name(new_state) 
              << " (from " << get_state_name(g_robot_state.previous_state) << ")" << std::endl;
    
    // Handle state-specific actions
    switch (new_state) {
        case RobotStateCode::EMERGENCY_STOP:
            g_robot_state.last_message = "Emergency Stop Activated!";
            break;
        case RobotStateCode::THREAT_DETECTED:
            g_robot_state.last_message = "Threat Detected!";
            break;
        case RobotStateCode::OBSTACLE_AVOID:
            g_robot_state.last_message = "Avoiding Obstacle...";
            break;
        default:
            break;
    }
}

// ============================================================
// Get State
// ============================================================

RobotStateCode get_state() {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    return g_robot_state.current_state;
}

// ============================================================
// Check if Robot Can Move
// ============================================================

bool is_ok_to_move() {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    return g_robot_state.current_state == RobotStateCode::PATROL ||
           g_robot_state.current_state == RobotStateCode::MAPPING ||
           g_robot_state.current_state == RobotStateCode::RETURN_HOME;
}

// ============================================================
// Check Emergency
// ============================================================

bool is_emergency() {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    return g_robot_state.current_state == RobotStateCode::EMERGENCY_STOP;
}

// ============================================================
// Pause Control
// ============================================================

void set_paused(bool paused) {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    if (g_robot_state.paused != paused) {
        g_robot_state.paused = paused;
        if (paused) {
            g_robot_state.previous_state = g_robot_state.current_state;
            g_robot_state.current_state = RobotStateCode::PAUSED;
            std::cout << "[STATE] ⏸️ PAUSED" << std::endl;
        } else {
            g_robot_state.current_state = g_robot_state.previous_state;
            std::cout << "[STATE] ▶️ Resumed: " << get_state_name(g_robot_state.current_state) << std::endl;
        }
    }
}

bool is_paused() {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    return g_robot_state.paused || g_robot_state.current_state == RobotStateCode::PAUSED;
}

// ============================================================
// Running Control
// ============================================================

void set_running(bool running) {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    g_robot_state.running = running;
    if (!running) {
        g_robot_state.current_state = RobotStateCode::SHUTDOWN;
        std::cout << "[STATE] 🔴 SHUTDOWN" << std::endl;
    }
}

bool is_running() {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    return g_robot_state.running;
}

// ============================================================
// Pose Updates
// ============================================================

void update_pose(float x, float y, float theta) {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    g_robot_state.x = x;
    g_robot_state.y = y;
    g_robot_state.theta = theta;
}

// ============================================================
// Battery Updates
// ============================================================

void update_battery(float voltage) {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    g_robot_state.battery_voltage = voltage;
    g_robot_state.battery_low = voltage < 11.5f;
    
    if (g_robot_state.battery_low) {
        g_robot_state.last_message = "⚠️ BATTERY LOW: " + std::to_string(voltage) + "V";
        std::cout << "[BATTERY] ⚠️ Low voltage: " << voltage << "V" << std::endl;
    }
}

// ============================================================
// Detection Updates
// ============================================================

void update_detection(int person, int weapon, int fire) {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    g_robot_state.person_count = person;
    g_robot_state.weapon_count = weapon;
    g_robot_state.fire_count = fire;
    g_robot_state.person_detected = person > 0;
    g_robot_state.weapon_detected = weapon > 0;
    g_robot_state.fire_detected = fire > 0;
}

// ============================================================
// FPS Updates
// ============================================================

void update_fps(float fps) {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    g_robot_state.fps = fps;
}

// ============================================================
// System Metrics Updates
// ============================================================

void update_cpu_usage(float cpu) {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    g_robot_state.cpu_usage = cpu;
}

void update_temperature(float temp) {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    g_robot_state.temperature = temp;
}

// ============================================================
// State Transition Helpers
// ============================================================

void on_threat_detected() {
    // Called when AI detects a threat
    std::cout << "[STATE] ⚠️ Threat detected, switching to THREAT_DETECTED" << std::endl;
    set_state(RobotStateCode::THREAT_DETECTED);
    // The main loop will handle the alert
}

void on_emergency_detected() {
    // Called when emergency (fire, collision) detected
    std::cout << "[STATE] 🚨 EMERGENCY detected!" << std::endl;
    set_state(RobotStateCode::EMERGENCY_STOP);
}

void on_obstacle_detected() {
    // Called by ESP32 when obstacle detected
    std::cout << "[STATE] 🚧 Obstacle detected, switching to OBSTACLE_AVOID" << std::endl;
    set_state(RobotStateCode::OBSTACLE_AVOID);
}

void on_avoidance_complete() {
    // Called by ESP32 when avoidance complete
    std::cout << "[STATE] ✅ Avoidance complete" << std::endl;
    // Resume previous state
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    if (g_robot_state.current_state == RobotStateCode::OBSTACLE_AVOID) {
        RobotStateCode resume_state = g_robot_state.previous_state;
        // Don't resume to emergency states
        if (resume_state == RobotStateCode::EMERGENCY_STOP ||
            resume_state == RobotStateCode::THREAT_DETECTED) {
            resume_state = RobotStateCode::IDLE;
        }
        set_state(resume_state);
    }
}

void on_command_patrol() {
    // Called by Telegram /patrol command
    std::cout << "[STATE] 🚀 Starting patrol" << std::endl;
    set_state(RobotStateCode::PATROL);
}

void on_command_stop() {
    // Called by Telegram /stop command
    std::cout << "[STATE] ⏸️ Pausing robot" << std::endl;
    set_paused(true);
}

void on_command_resume() {
    // Called by Telegram /resume command
    std::cout << "[STATE] ▶️ Resuming robot" << std::endl;
    set_paused(false);
}

void on_command_return_home() {
    // Called by Telegram /return command
    std::cout << "[STATE] 🏠 Returning home" << std::endl;
    set_state(RobotStateCode::RETURN_HOME);
}

// ============================================================
// State Reset (for emergency override)
// ============================================================

void reset_state() {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    g_robot_state.current_state = RobotStateCode::IDLE;
    g_robot_state.previous_state = RobotStateCode::IDLE;
    g_robot_state.paused = false;
    g_robot_state.obstacle_detected = false;
    g_robot_state.is_avoiding = false;
    g_robot_state.person_detected = false;
    g_robot_state.weapon_detected = false;
    g_robot_state.fire_detected = false;
    g_robot_state.last_message = "State reset";
    std::cout << "[STATE] 🔄 State reset to IDLE" << std::endl;
}

// ============================================================
// State Status String (for Telegram /status)
// ============================================================

std::string get_state_status_string() {
    std::lock_guard<std::mutex> lock(g_robot_state.mutex);
    
    std::string status = "";
    status += "🔋 Battery: " + std::to_string(g_robot_state.battery_voltage) + "V";
    if (g_robot_state.battery_low) status += " ⚠️ LOW!";
    status += "\n";
    
    status += "📍 Position: (" + std::to_string(g_robot_state.x) + ", " + 
              std::to_string(g_robot_state.y) + ")\n";
    
    status += "🧭 Heading: " + std::to_string(g_robot_state.theta * 180.0 / 3.14159) + "°\n";
    
    status += "🧠 State: " + std::string(get_state_name(g_robot_state.current_state)) + "\n";
    
    status += "👤 Persons: " + std::to_string(g_robot_state.person_count) + "\n";
    status += "🔫 Weapons: " + std::to_string(g_robot_state.weapon_count) + "\n";
    status += "🔥 Fires: " + std::to_string(g_robot_state.fire_count) + "\n";
    
    status += "📊 CPU: " + std::to_string(g_robot_state.cpu_usage) + "%\n";
    status += "🌡️ Temp: " + std::to_string(g_robot_state.temperature) + "°C\n";
    status += "📸 FPS: " + std::to_string(g_robot_state.fps) + "\n";
    
    status += "⏸️ Paused: " + std::string(g_robot_state.paused ? "Yes" : "No") + "\n";
    status += "🚧 Obstacle: " + std::string(g_robot_state.obstacle_detected ? "Yes" : "No");
    
    return status;
}