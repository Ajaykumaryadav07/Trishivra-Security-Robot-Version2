#include "robot_modules.h"
#include "yolo_detector.h"
#include "robot_protocol.h"
#include "robot_state.h"
#include "voice_controller.h"
#include "config.h"
#include "utils.h"
#include "gps_receiver.h"
#include "imu_mpu6050.h"
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
#include <vector>
#include <cmath>
#include "odometry.h"


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
            file << "\"vehicle_count\":" << state.vehicle_count << ",";
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
    state.vehicle_count = 0;
    state.fire_count = 0;
    state.person_detected = false;
    state.weapon_detected = false;
    state.fire_detected = false;

    static auto last_vehicle_seen = std::chrono::steady_clock::now();
    static bool missing_alert_sent = false;
    const cv::Rect parking_roi(
        PARKING_ROI_X, PARKING_ROI_Y, PARKING_ROI_WIDTH, PARKING_ROI_HEIGHT);
    bool vehicle_in_parking = false;
    
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
            
        } else if (det.class_name == "vehicle") {
            const cv::Point vehicle_center(
                det.bbox.x + det.bbox.width / 2,
                det.bbox.y + det.bbox.height / 2);
            if (parking_roi.contains(vehicle_center)) {
                state.vehicle_count++;
                vehicle_in_parking = true;
            }
        } else if (det.class_name == "fire") {
            state.fire_count++;
            state.fire_detected = true;
            set_state(RobotStateCode::EMERGENCY_STOP);
            
            if (elapsed > ALERT_COOLDOWN_MS) {
                std::cout << "[MAIN] 🔥 FIRE detected! Confidence: " 
                          << det.confidence << std::endl;
                
                std::ofstream trigger_file(ALERT_FILE());
                if (trigger_file.is_open()) {
                    trigger_file << "FIRE_DETECTED\n";
                    trigger_file << "Confidence: " << det.confidence << "\n";
                    trigger_file.close();
                }
                voice_speak("Fire detected! Emergency stop!");
                last_alert_time = now;
            }
        }
    }

    if (vehicle_in_parking) {
        last_vehicle_seen = now;
        missing_alert_sent = false;
    } else {
        const auto missing_for = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_vehicle_seen).count();
        if (missing_for >= VEHICLE_MISSING_TIMEOUT_SECONDS && !missing_alert_sent) {
            std::ofstream trigger_file(ALERT_FILE());
            if (trigger_file.is_open()) {
                trigger_file << "VEHICLE_MISSING\n";
                trigger_file << "Vehicle absent from parking for: "
                             << missing_for << " seconds\n";
                trigger_file.close();
            }
            std::cout << "[MAIN] Vehicle missing from parking" << std::endl;
            missing_alert_sent = true;
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
    std::cout << "   10 FPS | Pure C++ Core" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    g_start_time = std::chrono::steady_clock::now();
    
    // ============================================================
    // 1. Initialize Camera (10 FPS)
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
    if (!yolo.init(std::string(YOLO_MODEL_PATH), YOLO_CONFIDENCE_THRESHOLD)) {
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
    // 5. Initialize Navigation
    // ============================================================
    NavigationModule nav;
    GPSReceiver gps;
    const bool gps_ready = gps.open("/dev/serial0", 9600);
    if (!gps_ready) {
        std::cerr << "[MAIN] GPS unavailable; autonomous motors remain stopped"
                  << std::endl;
    }
    
    std::vector<Pose> waypoints;
    std::string navigation_mode;
    bool autonomous_movement = false;
    bool require_gps = true;
    double initial_heading_deg = 0.0;
    double gps_origin_latitude = 0.0;
    double gps_origin_longitude = 0.0;
    const bool route_ready = load_navigation_waypoints(
        "config/navigation_waypoints.yaml", waypoints, navigation_mode,
        autonomous_movement, require_gps, initial_heading_deg,
        gps_origin_latitude, gps_origin_longitude);
    nav.set_waypoints(waypoints);
    g_robot_state.total_waypoints = waypoints.size();
    motor.stop();
    MPU6050 imu;
    const bool imu_ready = imu.initialize("/dev/i2c-1", 0x68,
                                          initial_heading_deg);
    if (route_ready) {
        std::cout << "[MAIN] GPS waypoints loaded: " << waypoints.size() << std::endl;
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
    
    // ============================================================
    // Main Loop Variables
    // ============================================================
    const int FRAME_INTERVAL_MS = 1000 / CAMERA_FPS;
    int frame_count = 0;
    auto last_fps_update = std::chrono::steady_clock::now();
    int fps_counter = 0;
    
    // ============================================================
    // Start Patrol
    // ============================================================
    const bool patrol_ready = gps_ready && imu_ready && route_ready && require_gps &&
                              autonomous_movement && navigation_mode == "hardware";
    if (patrol_ready) {
        set_state(RobotStateCode::PATROL);
        std::cout << "[MAIN] Starting autonomous patrol" << std::endl;
        voice_speak("Patrol mode started");
    } else {
        set_state(RobotStateCode::IDLE);
        std::cout << "[MAIN] Motors remain disabled: require GPS, MPU-6050, "
                     "hardware mode, autonomous_movement: 1, and an enabled route"
                  << std::endl;
    }

    GPSFix gps_fix;
    bool gps_has_fix = false;
    bool start_position_verified = false;
    bool imu_heading_valid = imu_ready;
    bool imu_failure_reported = false;
    
    while (g_running && g_robot_state.running) {
        auto frame_start = std::chrono::steady_clock::now();

        GPSFix updated_fix;
        if (gps.poll(updated_fix)) {
            gps_fix = updated_fix;
            gps_has_fix = true;
        }
        
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
            motor.stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_INTERVAL_MS));
            continue;
        }
        
        // ============================================================
        // 2. OBJECT DETECTION (YOLO)
        // ============================================================
        if (yolo.is_initialized()) {
            auto detections = yolo.detect(frame);
            
            // Handle detections
            handle_detections(detections, g_robot_state, frame);
        }
        
        // ============================================================
        // 3. UPDATE GPS POSE
        // ============================================================
        if (imu_heading_valid && !imu.update()) {
            imu_heading_valid = false;
            motor.stop();
            if (!imu_failure_reported) {
                std::cerr << "[IMU] Heading unavailable; stopping autonomous motion"
                          << std::endl;
                imu_failure_reported = true;
            }
        }

        const auto now_steady = std::chrono::steady_clock::now();
        const bool gps_fix_fresh = gps_has_fix &&
            now_steady - gps_fix.received_at <= std::chrono::seconds(3);
        bool near_route_start = false;
        if (gps_fix_fresh && !waypoints.empty()) {
            constexpr double meters_per_degree = 111320.0;
            const double east = (gps_fix.longitude - gps_origin_longitude) *
                meters_per_degree * std::cos(gps_origin_latitude * M_PI / 180.0);
            const double north = (gps_fix.latitude - gps_origin_latitude) *
                meters_per_degree;
            const double distance_from_start = std::hypot(east, north);
            near_route_start = distance_from_start <= 4.0;
            if (route_ready && !start_position_verified && near_route_start) {
                start_position_verified = true;
                std::cout << "[GPS] Route start confirmed" << std::endl;
            }
            const double heading_rad = imu.heading_rad();
            const Pose pose{
                static_cast<float>(east),
                static_cast<float>(north),
                static_cast<float>(heading_rad)
            };
            nav.update_pose(pose);
            g_robot_state.x = pose.x;
            g_robot_state.y = pose.y;
            g_robot_state.theta = pose.theta;
        } else {
            g_robot_state.x = 0.0f;
            g_robot_state.y = 0.0f;
        }
        
        // ============================================================
        // 5. CHECK OBSTACLES (ESP32)
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
        // 6. NAVIGATE
        // ============================================================
        if (is_emergency()) {
            motor.stop();
            std::cout << "[MAIN] 🚨 EMERGENCY STOP!" << std::endl;

            if (g_robot_state.fire_detected) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                g_robot_state.fire_detected = false;
                set_state(RobotStateCode::IDLE);
                voice_speak("Fire emergency cleared");
            }
        } else if (motor.is_avoiding()) {
            // ESP32 owns the active avoidance maneuver; don't interrupt it with STOP.
        } else if (gps_fix_fresh && start_position_verified && patrol_ready &&
                   imu_heading_valid &&
                   is_ok_to_move() && !g_robot_state.paused) {
            if (g_robot_state.obstacle_detected) {
                nav.clear_path();
                motor.stop();
            } else {
                VelocityCmd cmd = nav.get_velocity();
                g_robot_state.current_waypoint = nav.get_current_waypoint_index();
                if (nav.is_route_complete()) {
                    motor.stop();
                    set_state(RobotStateCode::IDLE);
                    std::cout << "[MAIN] ✅ Patrol route complete; robot stopped" << std::endl;
                    voice_speak("Patrol complete");
                } else {
                    motor.set_gps_motion(cmd.linear, cmd.angular);
                }
            }
        } else {
            motor.stop();
            if (patrol_ready && gps_fix_fresh &&
                !start_position_verified && !near_route_start) {
                std::cerr << "[GPS] Robot is more than 12 m from the configured "
                             "route start; autonomous movement is inhibited"
                          << std::endl;
            }
        }
        
        // ============================================================
        // 7. UPDATE SYSTEM METRICS
        // ============================================================
        update_system_metrics(g_robot_state);
        
        // ============================================================
        // 8. PUBLISH STATUS (for Python/Telegram)
        // ============================================================
        if (frame_count % 10 == 0) {
            publish_status(g_robot_state);
        }
        
        // ============================================================
        // 9. FPS TRACKING
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
        // 10. FPS CONTROL (10 FPS)
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