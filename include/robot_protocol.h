#ifndef ROBOT_PROTOCOL_H
#define ROBOT_PROTOCOL_H

#include <string>
#include <vector>
#include <cstdint>

// ===== SERIAL COMMANDS =====
namespace RobotProtocol {
    // Motor Commands
    const char CMD_FORWARD = 'F';
    const char CMD_BACKWARD = 'B';
    const char CMD_TURN_LEFT = 'L';
    const char CMD_TURN_RIGHT = 'R';
    const char CMD_STOP = 'S';
    const char CMD_VIRTUAL_OBSTACLE_LEFT = 'O';
    const char CMD_VIRTUAL_OBSTACLE_RIGHT = 'P';
    const char CMD_STEERING_ANGLE = 'C';
    const char CMD_GET_STATUS = '?';
    
    // ESP32 Response Types
    const std::string RESP_OBSTACLE_DETECTED = "OBSTACLE_DETECTED";
    const std::string RESP_AVOIDANCE_COMPLETE = "AVOIDANCE_COMPLETE";
    const std::string RESP_TURNING_LEFT = "TURNING_LEFT";
    const std::string RESP_TURNING_RIGHT = "TURNING_RIGHT";
    const std::string RESP_CMD_FORWARD = "CMD_FORWARD";
    const std::string RESP_CMD_BACKWARD = "CMD_BACKWARD";
    const std::string RESP_CMD_LEFT = "CMD_LEFT_TURN";
    const std::string RESP_CMD_RIGHT = "CMD_RIGHT_TURN";
    const std::string RESP_CMD_STOP = "CMD_STOP";
    const std::string RESP_ANGLE_SET = "ANGLE_SET";
    const std::string RESP_EMERGENCY = "EMERGENCY";
}

// ===== TELEGRAM PROTOCOL =====
namespace TelegramProtocol {
    const std::string CMD_START = "/start";
    const std::string CMD_STOP = "/stop";
    const std::string CMD_RESUME = "/resume";
    const std::string CMD_STATUS = "/status";
    const std::string CMD_PATROL = "/patrol";
    const std::string CMD_OBSTACLE_LEFT = "/obstacle_left";
    const std::string CMD_OBSTACLE_RIGHT = "/obstacle_right";
}

// ===== STATE MACHINE PROTOCOL =====
enum class RobotStateCode : uint8_t {
    IDLE = 0,
    PATROL = 1,
    MAPPING = 2,
    THREAT_DETECTED = 3,
    EMERGENCY_STOP = 4,
    RETURN_HOME = 5,
    OBSTACLE_AVOID = 6,
    PAUSED = 7,
    SHUTDOWN = 8
};

// ===== SENSOR DATA STRUCTURE =====
struct SensorData {
    float front_distance;      // cm
    float left_distance;       // cm
    float right_distance;      // cm
    float battery_voltage;     // V
    float imu_yaw;             // degrees
    int left_encoder_ticks;
    int right_encoder_ticks;
    bool emergency_stop;
};

// ===== STATUS MESSAGE =====
struct RobotStatus {
    RobotStateCode state;
    float x, y, theta;         // Pose
    float battery_voltage;
    float cpu_usage;
    float temperature;
    float fps;
    int person_count;
    int weapon_count;
    int fire_count;
    bool obstacle_detected;
    int current_waypoint;
    int total_waypoints;
};

#endif // ROBOT_PROTOCOL_H