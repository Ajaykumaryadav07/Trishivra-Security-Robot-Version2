// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <string_view>
#include <cstdlib>

// Platform-aware temporary directory helper (optimized)
inline constexpr std::string_view get_temp_dir_unix() { return "/tmp"; }
inline constexpr std::string_view get_temp_dir_win() { return "C:/Temp"; }

// Shared file paths (optimized with string_view for faster access)
inline constexpr std::string_view STATUS_FILE_PATH = "/tmp/robot_status.json";
inline constexpr std::string_view ALERT_FILE_PATH = "/tmp/alert_trigger.txt";
inline constexpr std::string_view COMMAND_FILE_PATH = "/tmp/robot_command.txt";
inline constexpr std::string_view DETECTION_FILE_PATH = "/tmp/detection_state.json";
inline constexpr std::string_view LOCATION_FILE_PATH = "/tmp/current_location.txt";
inline constexpr std::string_view FLAG_FILE_PATH = "/tmp/alert_flag.txt";

// Keep legacy functions for compatibility
inline std::string get_temp_dir() {
#ifdef _WIN32
    return "C:/Temp";
#else
    return "/tmp";
#endif
}
inline std::string STATUS_FILE() { return std::string(STATUS_FILE_PATH); }
inline std::string ALERT_FILE()  { return std::string(ALERT_FILE_PATH); }
inline std::string COMMAND_FILE(){ return std::string(COMMAND_FILE_PATH); }
inline std::string DETECTION_FILE(){ return std::string(DETECTION_FILE_PATH); }
inline std::string LOCATION_FILE(){ return std::string(LOCATION_FILE_PATH); }
inline std::string FLAG_FILE(){ return std::string(FLAG_FILE_PATH); }

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Camera defaults (optimized)
constexpr int CAMERA_WIDTH = 640;
constexpr int CAMERA_HEIGHT = 480;
constexpr int CAMERA_FPS = 6;
constexpr int FRAME_INTERVAL_MS = 1000 / CAMERA_FPS;  // Pre-calculated

// YOLO defaults (removed duplicates, optimized)
constexpr std::string_view YOLO_MODEL_PATH = "models/best_openvino_model";
constexpr float YOLO_CONFIDENCE_THRESHOLD = 0.5f;
constexpr int YOLO_IMG_SIZE = 320;

// Navigation (optimized constants)
constexpr float WAYPOINT_TOLERANCE = 0.1f;
constexpr float MAP_WIDTH = 10.0f;      // meters
constexpr float MAP_HEIGHT = 10.0f;     // meters
constexpr float MAP_RESOLUTION = 0.1f;  // 0.1m per cell (optimized from 0.05m)
constexpr float MAX_SPEED = 0.5f;       // m/s (conservative for safety)
constexpr float MAX_TURN_SPEED = 1.0f;  // rad/s

// Steering PID defaults
constexpr float KP = 0.8f;
constexpr float KI = 0.01f;
constexpr float KD = 0.05f;
constexpr int POT_MIN = 0;
constexpr int POT_MAX = 1023;
constexpr int STRAIGHT_ANGLE = 90;
constexpr int STEER_MAX_DELTA = 10;
constexpr float STEER_UPDATE_HZ = 50.0f;

// Serial defaults
#ifdef _WIN32
constexpr const char* SERIAL_PORT = "COM3";
#else
constexpr const char* SERIAL_PORT = "/dev/ttyUSB0";
#endif
constexpr int BAUD_RATE = 115200;

// Misc
constexpr int ALERT_COOLDOWN_MS = 5000;

#endif // CONFIG_H
