#include "robot_modules.h"
#include "yolo_detector.h"
#include "robot_protocol.h"
#include "robot_state.h"
#include "voice_controller.h"
#include "config.h"
#include "utils.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <cstdio>
#include "odometry.h"
#include "camera.h"


// ============================================================
// Global Variables
// ============================================================

volatile bool g_running = true;
std::atomic<bool> g_paused{false};
std::chrono::steady_clock::time_point g_start_time;

// ============================================================
// Signal Handler
// ============================================================

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        std::cout << "\n[MAIN] Shutdown signal received..." << std::endl;
        g_running = false;
        set_state(RobotStateCode::SHUTDOWN);
    }
}

// ============================================================
// Status Publisher (for Python Telegram bridge)
// ============================================================

void publish_status(const RobotStateData& state) {
    try {
        std::ofstream file(STATUS_FILE());
        if (file.is_open()) {
            file << "{";
            file << "\"state\":\"" << get_state_name(state.current_state) << "\",";
            file << "\"x\":" << std::fixed << std::setprecision(2) << state.x << ",";
            file << "\"y\":" << state.y << ",";
            file << "\"theta\":" << state.theta << ",";
            file << "\"battery\":" << state.battery_voltage << ",";
            file << "\"person_count\":" << state.person_count << ",";
            file << "\"weapon_count\":" << state.weapon_count << ",";
            file << "\"fire_count\":" << state.fire_count << ",";
            file << "\"cpu\":" << state.cpu_usage << ",";
            file << "\"fps\":" << state.fps << ",";
            file << "\"paused\":" << (state.paused ? "true" : "false") << ",";
            file << "\"obstacle\":" << (state.obstacle_detected ? "true" : "false") << ",";
            file << "\"temperature\":" << state.temperature;
            file << "}";
            file.close();
        }
    } catch (...) {
        // Ignore errors
    }
}

// ============================================================
// Update System Metrics
// ============================================================

void update_system_metrics(RobotStateData& state) {
#ifndef _WIN32
    // Read CPU usage from /proc/stat
    static unsigned long prev_idle = 0, prev_total = 0;
    
    std::ifstream stat_file("/proc/stat");
    if (stat_file.is_open()) {
        std::string line;
        std::getline(stat_file, line);
        stat_file.close();
        
        unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
        sscanf(line.c_str(), "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
        
        unsigned long total = user + nice + system + idle + iowait + irq + softirq + steal;
        unsigned long total_diff = total - prev_total;
        unsigned long idle_diff = idle - prev_idle;
        
        if (total_diff > 0) {
            state.cpu_usage = 100.0f * (1.0f - (float)idle_diff / total_diff);
        }
        
        prev_idle = idle;
        prev_total = total;
    }
    
    // Read temperature
    std::ifstream temp_file("/sys/class/thermal/thermal_zone0/temp");
    if (temp_file.is_open()) {
        int temp_raw;
        temp_file >> temp_raw;
        state.temperature = temp_raw / 1000.0f;
        temp_file.close();
    }
#endif
    
    // Simulate battery voltage (in production, read from ESP32)
    static float simulated_battery = 12.4f;
    static int counter = 0;
    counter++;
    if (counter % 100 == 0) {
        simulated_battery -= 0.01f;
        if (simulated_battery < 11.0f) simulated_battery = 12.4f;
        state.battery_voltage = simulated_battery;
        state.battery_low = simulated_battery < 11.5f;
    }
}

// ============================================================
// Handle Detection Results (NEW: Improved)
// ============================================================

bool read_robot_command(std::string& command, std::string& value) {
    std::ifstream file(COMMAND_FILE());
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    if (!std::getline(file, line) || line.empty()) {
        return false;
    }

    file.close();
    std::remove(COMMAND_FILE().c_str());

    std::istringstream iss(line);
    iss >> command;
    std::getline(iss, value);
    if (!value.empty() && value.front() == ' ') {
        value.erase(0, 1);
    }
    return !command.empty();
}

extern void on_command_patrol();
extern void on_command_stop();
extern void on_command_resume();
extern void on_command_return_home();

void process_robot_command(const std::string& command, const std::string& value) {
    if (command == "STOP" || command == "PAUSE") {
        on_command_stop();
    } else if (command == "RESUME" || command == "START") {
        on_command_resume();
    } else if (command == "PATROL" || command == "AUTONOMOUS") {
        on_command_patrol();
    } else if (command == "RETURN_HOME") {
        on_command_return_home();
    } else if (command == "SHUTDOWN" || command == "EXIT") {
        set_running(false);
    } else {
        std::cout << "[MAIN] Unknown robot command: " << command << std::endl;
    }
}

void handle_detections(const std::vector<Detection>& detections, 
                       RobotStateData& state, 
                       const cv::Mat& frame) {
    static std::chrono::steady_clock::time_point last_alert_time;
    static const int ALERT_COOLDOWN_MS = 5000;  // 5 seconds
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_alert_time).count();
    
    // Reset counts each frame
    state.person_count = 0;
    state.weapon_count = 0;
    state.fire_count = 0;
    
    for (const auto& det : detections) {
        if (det.class_name == "person") {
            state.person_count++;
            state.person_detected = true;
            
            if (elapsed > ALERT_COOLDOWN_MS) {
                std::cout << "[MAIN] 👤 Person detected! Confidence: " 
                          << det.confidence << std::endl;
                
                // Save alert frame
                std::string alert_file = get_temp_dir() + "/alert_" + std::to_string(std::time(nullptr)) + ".jpg";
                cv::imwrite(alert_file, frame);
                
                // Trigger alert via file
                std::ofstream trigger_file(ALERT_FILE());
                if (trigger_file.is_open()) {
                    trigger_file << "PERSON_DETECTED\n";
                    trigger_file << "Confidence: " << det.confidence << "\n";
                    trigger_file << "File: " << alert_file << "\n";
                    trigger_file.close();
                }
                
                last_alert_time = now;
            }
            
        } else if (det.class_name == "weapon") {
            state.weapon_count++;
            state.weapon_detected = true;
            
            if (elapsed > ALERT_COOLDOWN_MS) {
                std::cout << "[MAIN] 🔫 WEAPON detected! Confidence: " 
                          << det.confidence << std::endl;
                
                std::ofstream trigger_file(ALERT_FILE());
                if (trigger_file.is_open()) {
                    trigger_file << "WEAPON_DETECTED\n";
                    trigger_file << "Confidence: " << det.confidence << "\n";
                    trigger_file.close();
                }
                set_state(RobotStateCode::THREAT_DETECTED);
                last_alert_time = now;
            }
            
        } else if (det.class_name == "fire") {
            state.fire_count++;
            state.fire_detected = true;
            
            if (elapsed > ALERT_COOLDOWN_MS) {
                std::cout << "[MAIN] 🔥 FIRE detected! Confidence: " 
                          << det.confidence << std::endl;
                
                std::ofstream trigger_file(ALERT_FILE());
                if (trigger_file.is_open()) {
                    trigger_file << "FIRE_DETECTED\n";
                    trigger_file << "Confidence: " << det.confidence << "\n";
                    trigger_file.close();
                }
                set_state(RobotStateCode::EMERGENCY_STOP);
                voice_speak("Fire detected! Emergency stop!");
                last_alert_time = now;
            }
        }
    }
}

// ============================================================
// Main Function
// ============================================================

int main() {
    // ============================================================
    // Setup Signal Handlers
    // ============================================================
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "========================================" << std::endl;
    std::cout << "   TRISHIVRA SECURITY ROBOT V2" << std::endl;
    std::cout << "   No ROS2 | No LiDAR | No Jetson" << std::endl;
    std::cout << "   6 FPS | Pure C++ Core" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    g_start_time = std::chrono::steady_clock::now();
    
    // ============================================================
    // 1. Initialize Camera (6 FPS)
    // ============================================================
    CameraModule camera;
    if (!camera.init(CAMERA_WIDTH, CAMERA_HEIGHT, CAMERA_FPS)) {
        std::cerr << "[MAIN] ❌ Camera init failed!" << std::endl;
        return 1;
    }
    
    // ============================================================
    // 1b. Enable Night Vision (Auto Day/Night Switching)
    // ============================================================
    camera.enable_night_vision(true);
    camera.enable_auto_brightness(true);  // Auto switch between day/night modes
    std::cout << "[MAIN] 🌙 Night Vision enabled with auto brightness detection" << std::endl;
    
    // ============================================================
    // 2. Initialize YOLO (C++ Version)
    // ============================================================
    YOLODetector yolo;
    if (!yolo.init(YOLO_MODEL_PATH, YOLO_CONFIDENCE_THRESHOLD)) {
        std::cerr << "[MAIN] ⚠️ YOLO init failed!" << std::endl;
        std::cout << "[MAIN] Running without YOLO detection" << std::endl;
    }
    
    // ============================================================
    // 3. Initialize Motor Controller
    // ============================================================
    MotorController motor;
    if (!motor.init(SERIAL_PORT, BAUD_RATE)) {
        std::cerr << "[MAIN] ❌ Motor init failed!" << std::endl;
        return 1;
    }
    
    // ============================================================
    // 4. Initialize SLAM
    // ============================================================
    SLAMModule slam;
    if (!slam.init("data/ORBvoc.txt", "config/slam_params.yaml")) {
        std::cerr << "[MAIN] ⚠️ SLAM init failed!" << std::endl;
        std::cout << "[MAIN] Running without SLAM" << std::endl;
    }
    
    // ============================================================
    // 5. Initialize Navigation
    // ============================================================
    NavigationModule nav;
    
    // Define waypoints (adjust based on your environment)
    std::vector<Pose> waypoints = {
        {1.0f, 0.0f, 0.0f},
        {2.0f, 2.0f, 0.0f},
        {0.0f, 2.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };
    nav.set_waypoints(waypoints);
    g_robot_state.total_waypoints = waypoints.size();
    std::cout << "[MAIN] 🗺️ Waypoints loaded: " << waypoints.size() << std::endl;
    
    // ============================================================
    // 6. Initialize Depth Estimator
    // ============================================================
    DepthEstimator depth;
    if (!depth.is_initialized()) {
        std::cout << "[MAIN] ⚠️ Depth estimator not available" << std::endl;
    }
    
    // ============================================================
    // 7. Initialize Voice (Optional)
    // ============================================================
    voice_auto_start();
    
    // ============================================================
    // 8. Initialize State
    // ============================================================
    set_state(RobotStateCode::IDLE);
    g_robot_state.paused = false;
    g_robot_state.running = true;
    
    std::cout << "\n[MAIN] ✅ Robot initialized!" << std::endl;
    std::cout << "[MAIN] 🚀 Starting autonomous patrol..." << std::endl;
    std::cout << std::endl;
    
    // ============================================================
    // Main Loop Variables
    // ============================================================
    const int FRAME_INTERVAL_MS = 166;  // 1000ms / 6 FPS
    int frame_count = 0;
    auto last_fps_update = std::chrono::steady_clock::now();
    int fps_counter = 0;
    int waypoint_progress = 0;
    
    // ============================================================
    // Start Patrol
    // ============================================================
    set_state(RobotStateCode::PATROL);
    voice_speak("Patrol mode started");
    
    while (g_running && g_robot_state.running) {
        auto frame_start = std::chrono::steady_clock::now();
        
        {
            std::string command, value;
            if (read_robot_command(command, value)) {
                std::cout << "[MAIN] Received robot command: " << command;
                if (!value.empty()) {
                    std::cout << " " << value;
                }
                std::cout << std::endl;
                process_robot_command(command, value);
            }
        }

        // ============================================================
        // Check Pause
        // ============================================================
        if (g_robot_state.paused) {
            motor.stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // ============================================================
        // 1. CAPTURE FRAME
        // ============================================================
        cv::Mat frame;
        if (!camera.capture(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_INTERVAL_MS));
            continue;
        }
        
        // ============================================================
        // 2. DEPTH ESTIMATION
        // ============================================================
        cv::Mat depth_map = depth.estimate(frame);
        
        // ============================================================
        // 3. OBJECT DETECTION (YOLO)
        // ============================================================
        if (yolo.is_initialized()) {
            auto detections = yolo.detect(frame);
            
            // Handle detections
            if (!detections.empty()) {
                handle_detections(detections, g_robot_state, frame);
            }
        }
        
        // ============================================================
        // 4. UPDATE SLAM
        // ============================================================
        if (slam.is_initialized()) {
            Pose current_pose = nav.get_current_pose(); // Need to expose this
            slam.update(frame, current_pose);
            
            cv::Mat map = slam.get_map();
            if (!map.empty()) {
                nav.set_map(map);
            }
        }
        
        // ============================================================
        // 5. UPDATE POSE
        // ============================================================
        Pose pose = slam.get_pose();
        nav.update_pose(pose);
        
        g_robot_state.x = pose.x;
        g_robot_state.y = pose.y;
        g_robot_state.theta = pose.theta;
        
        // ============================================================
        // 6. CHECK OBSTACLES (ESP32)
        // ============================================================
        if (motor.is_avoiding()) {
            g_robot_state.obstacle_detected = true;
            set_state(RobotStateCode::OBSTACLE_AVOID);
        } else if (g_robot_state.obstacle_detected) {
            g_robot_state.obstacle_detected = false;
            if (get_state() == RobotStateCode::OBSTACLE_AVOID) {
                set_state(RobotStateCode::PATROL);
            }
        }
        
        // ============================================================
        // 7. NAVIGATE
        // ============================================================
        if (is_ok_to_move() && !g_robot_state.paused) {
            if (g_robot_state.obstacle_detected) {
                nav.clear_path();
            }
            VelocityCmd cmd = nav.get_velocity();
            
            // Check obstacle before moving
            if (!g_robot_state.obstacle_detected) {
                motor.set_speed(cmd.linear - cmd.angular, cmd.linear + cmd.angular);
            } else {
                motor.stop();
            }
            
            // Check if goal reached
            if (nav.is_goal_reached()) {
                waypoint_progress++;
                std::cout << "[MAIN] ✅ Waypoint " << waypoint_progress 
                          << " reached" << std::endl;
                
                // Next waypoint is auto-advanced by get_velocity()
            }
            
        } else if (is_emergency()) {
            motor.stop();
            std::cout << "[MAIN] 🚨 EMERGENCY STOP!" << std::endl;
            
            if (g_robot_state.fire_detected) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                g_robot_state.fire_detected = false;
                set_state(RobotStateCode::IDLE);
                voice_speak("Fire emergency cleared");
            }
        } else {
            motor.stop();
        }
        
        // ============================================================
        // 8. UPDATE SYSTEM METRICS
        // ============================================================
        update_system_metrics(g_robot_state);
        
        // ============================================================
        // 9. PUBLISH STATUS (for Python/Telegram)
        // ============================================================
        if (frame_count % 10 == 0) {
            publish_status(g_robot_state);
        }
        
        // ============================================================
        // 10. FPS TRACKING
        // ============================================================
        frame_count++;
        fps_counter++;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed_fps = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_update).count();
        
        if (elapsed_fps >= 1000) {
            g_robot_state.fps = fps_counter;
            fps_counter = 0;
            last_fps_update = now;
        }
        
        // ============================================================
        // 11. FPS CONTROL (6 FPS)
        // ============================================================
        auto frame_end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
        
        if (elapsed < FRAME_INTERVAL_MS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_INTERVAL_MS - elapsed));
        }
    }
    
    // ============================================================
    // Cleanup
    // ============================================================
    std::cout << "\n[MAIN] 🛑 Shutting down..." << std::endl;
    motor.stop();
    camera.release();
    voice_speak("Shutting down");
    voice_stop_listening();
    std::cout << "[MAIN] ✅ Shutdown complete." << std::endl;
    
    return 0;
}