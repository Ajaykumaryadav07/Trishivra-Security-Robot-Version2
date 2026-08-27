#ifndef ROBOT_STATE_H
#define ROBOT_STATE_H

#include <string>
#include <mutex>
#include <atomic>
#include "robot_protocol.h"

// ===== GLOBAL STATE STRUCT =====
struct RobotStateData {
    // State
    RobotStateCode current_state = RobotStateCode::IDLE;
    RobotStateCode previous_state = RobotStateCode::IDLE;
    
    // Position
    float x = 0.0f;
    float y = 0.0f;
    float theta = 0.0f;
    
    // Battery
    float battery_voltage = 12.4f;
    bool battery_low = false;
    
    // Detection
    bool person_detected = false;
    bool weapon_detected = false;
    bool fire_detected = false;
    int person_count = 0;
    int weapon_count = 0;
    int fire_count = 0;
    
    // Obstacles
    bool obstacle_detected = false;
    float obstacle_distance = 0.0f;
    bool is_avoiding = false;
    
    // System
    float cpu_usage = 0.0f;
    float temperature = 0.0f;
    float fps = 0.0f;
    std::string last_message;
    
    // Control
    bool paused = false;
    bool running = true;
    int current_waypoint = 0;
    int total_waypoints = 0;
    
    // Timestamp
    long long timestamp_ms = 0;
    
    // Mutex for thread safety
    std::mutex mutex;
};

// ===== GLOBAL INSTANCE =====
extern RobotStateData g_robot_state;

// ===== STATE FUNCTIONS =====
void set_state(RobotStateCode new_state);
RobotStateCode get_state();
bool is_ok_to_move();
bool is_emergency();
bool is_paused();
void set_paused(bool paused);
void set_running(bool running);
bool is_running();

// ===== COMMAND HANDLERS =====
void on_command_patrol();
void on_command_stop();
void on_command_resume();
void on_command_return_home();

// ===== STATE NAME HELPERS =====
const char* get_state_name(RobotStateCode state);
const char* get_state_name();

// ===== UPDATE FUNCTIONS =====
void update_pose(float x, float y, float theta);
void update_battery(float voltage);
void update_detection(int person, int weapon, int fire);
void update_fps(float fps);
void update_cpu_usage(float cpu);
void update_temperature(float temp);

#endif // ROBOT_STATE_H