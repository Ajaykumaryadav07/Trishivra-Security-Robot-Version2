#include "utils.h"
#include "config.h"
#include "robot_state.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <filesystem>

// ============================================================
// Math Utilities
// ============================================================

float normalize_angle(float angle) {
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
    return angle;
}

float clamp(float value, float min_val, float max_val) {
    return std::max(min_val, std::min(max_val, value));
}

int clamp_int(int value, int min_val, int max_val) {
    return std::max(min_val, std::min(max_val, value));
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx*dx + dy*dy);
}

float distance_pose(const Pose& a, const Pose& b) {
    return distance(a.x, a.y, b.x, b.y);
}

float angle_between(float x1, float y1, float x2, float y2) {
    return std::atan2(y2 - y1, x2 - x1);
}

float angle_difference(float a1, float a2) {
    float diff = a2 - a1;
    return normalize_angle(diff);
}

bool is_angle_close(float a1, float a2, float threshold) {
    return std::abs(normalize_angle(a2 - a1)) < threshold;
}

// ============================================================
// Time Utilities
// ============================================================

unsigned long get_time_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
}

unsigned long get_time_us() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()
    ).count();
}

std::string get_timestamp_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count() % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms;
    return ss.str();
}

std::string get_time_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    return ss.str();
}

float get_time_delta_ms(unsigned long start_time) {
    return static_cast<float>(get_time_ms() - start_time);
}

// ============================================================
// File Utilities
// ============================================================

bool file_exists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

bool read_file(const std::string& path, std::string& content) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    file.close();
    return true;
}

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    file.close();
    return true;
}

bool create_directory(const std::string& path) {
    try {
        std::filesystem::path p(path);
        return std::filesystem::create_directories(p) || std::filesystem::exists(p);
    } catch (...) {
        // Fallback to system call for older environments
#ifdef _WIN32
        std::string cmd = "mkdir \"" + path + "\"";
#else
        std::string cmd = "mkdir -p \"" + path + "\"";
#endif
        return system(cmd.c_str()) == 0;
    }
}

std::string get_file_name(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string get_file_extension(const std::string& path) {
    size_t pos = path.find_last_of('.');
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(pos + 1);
}

// ============================================================
// String Utilities
// ============================================================

std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

std::string trim_string(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string to_lower(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        c = std::tolower(c);
    }
    return result;
}

std::string to_upper(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        c = std::toupper(c);
    }
    return result;
}

bool starts_with(const std::string& str, const std::string& prefix) {
    if (str.length() < prefix.length()) {
        return false;
    }
    return str.compare(0, prefix.length(), prefix) == 0;
}

bool ends_with(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// ============================================================
// Image Utilities (OpenCV helpers)
// ============================================================

cv::Mat resize_image(const cv::Mat& image, int width, int height) {
    if (image.empty()) {
        return cv::Mat();
    }
    
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(width, height));
    return resized;
}

cv::Mat resize_image_aspect(const cv::Mat& image, int max_size) {
    if (image.empty()) {
        return cv::Mat();
    }
    
    int width = image.cols;
    int height = image.rows;
    
    if (width > height) {
        if (width > max_size) {
            float ratio = static_cast<float>(max_size) / width;
            width = max_size;
            height = static_cast<int>(height * ratio);
        }
    } else {
        if (height > max_size) {
            float ratio = static_cast<float>(max_size) / height;
            height = max_size;
            width = static_cast<int>(width * ratio);
        }
    }
    
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(width, height));
    return resized;
}

cv::Mat convert_to_grayscale(const cv::Mat& image) {
    if (image.empty()) {
        return cv::Mat();
    }
    
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }
    return gray;
}

cv::Mat apply_blur(const cv::Mat& image, int kernel_size) {
    if (image.empty()) {
        return cv::Mat();
    }
    
    cv::Mat blurred;
    cv::GaussianBlur(image, blurred, cv::Size(kernel_size, kernel_size), 0);
    return blurred;
}

void draw_pose(cv::Mat& image, const Pose& pose, const cv::Scalar& color) {
    if (image.empty()) {
        return;
    }
    
    // Scale factor for visualization
    float scale = 10.0f;
    
    // Center of image
    int cx = image.cols / 2;
    int cy = image.rows / 2;
    
    // Position
    int px = cx + static_cast<int>(pose.x * scale);
    int py = cy - static_cast<int>(pose.y * scale);
    
    // Draw circle at position
    cv::circle(image, cv::Point(px, py), 5, color, -1);
    
    // Draw heading arrow
    int len = 20;
    int end_x = px + static_cast<int>(len * std::cos(pose.theta));
    int end_y = py - static_cast<int>(len * std::sin(pose.theta));
    cv::arrowedLine(image, cv::Point(px, py), cv::Point(end_x, end_y), color, 2);
}

// ============================================================
// JSON Utilities (Simple)
// ============================================================

std::string json_encode(const std::string& key, const std::string& value) {
    return "\"" + key + "\":\"" + value + "\"";
}

std::string json_encode(const std::string& key, float value) {
    return "\"" + key + "\":" + std::to_string(value);
}

std::string json_encode(const std::string& key, int value) {
    return "\"" + key + "\":" + std::to_string(value);
}

std::string json_encode(const std::string& key, bool value) {
    return "\"" + key + "\":" + (value ? "true" : "false");
}

std::string json_escape(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

// ============================================================
// System Utilities
// ============================================================

float get_cpu_usage() {
    static unsigned long prev_idle = 0;
    static unsigned long prev_total = 0;
    
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        return 0.0f;
    }
    
    std::string line;
    std::getline(stat_file, line);
    stat_file.close();
    
    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
    sscanf(line.c_str(), "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    
    unsigned long total = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long total_diff = total - prev_total;
    unsigned long idle_diff = idle - prev_idle;
    
    prev_total = total;
    prev_idle = idle;
    
    if (total_diff == 0) {
        return 0.0f;
    }
    
    return 100.0f * (1.0f - static_cast<float>(idle_diff) / total_diff);
}

float get_cpu_temperature() {
    std::ifstream temp_file("/sys/class/thermal/thermal_zone0/temp");
    if (!temp_file.is_open()) {
        return 0.0f;
    }
    
    int temp_raw;
    temp_file >> temp_raw;
    temp_file.close();
    
    return temp_raw / 1000.0f;
}

float get_battery_voltage() {
    // Read from ADC via ESP32 or voltage divider
    // For now, return default value
    return g_robot_state.battery_voltage;
}

bool is_battery_low() {
    return g_robot_state.battery_voltage < 11.5f;
}

// ============================================================
// Odometry Simulation (if no encoders)
// ============================================================

/*
void update_odometry(float left_speed, float right_speed, float dt) {
    static float x = 0.0f, y = 0.0f, theta = 0.0f;
    static const float WHEEL_BASE = 0.3f;
    
    float v = (left_speed + right_speed) / 2.0f;
    float omega = (right_speed - left_speed) / WHEEL_BASE;
    
    x += v * std::cos(theta) * dt;
    y += v * std::sin(theta) * dt;
    theta += omega * dt;
    
    g_robot_state.x = x;
    g_robot_state.y = y;
    g_robot_state.theta = theta;
}
*/

// ============================================================
// Logging Utilities
// ============================================================

void log_message(const std::string& level, const std::string& module, const std::string& message) {
    std::string timestamp = get_time_string();
    std::cout << "[" << timestamp << "] [" << level << "] [" << module << "] " << message << std::endl;
}

void log_info(const std::string& module, const std::string& message) {
    log_message("INFO", module, message);
}

void log_warn(const std::string& module, const std::string& message) {
    log_message("WARN", module, message);
}

void log_error(const std::string& module, const std::string& message) {
    log_message("ERROR", module, message);
}

void log_debug(const std::string& module, const std::string& message) {
#ifdef DEBUG
    log_message("DEBUG", module, message);
#endif
}