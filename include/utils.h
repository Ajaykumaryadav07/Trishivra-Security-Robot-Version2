#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "robot_modules.h"

// ============================================================
// Math Utilities
// ============================================================

float normalize_angle(float angle);
float clamp(float value, float min_val, float max_val);
int clamp_int(int value, int min_val, int max_val);
float lerp(float a, float b, float t);
float distance(float x1, float y1, float x2, float y2);
float distance_pose(const Pose& a, const Pose& b);
float angle_between(float x1, float y1, float x2, float y2);
float angle_difference(float a1, float a2);
bool is_angle_close(float a1, float a2, float threshold);

// ============================================================
// Time Utilities
// ============================================================

unsigned long get_time_ms();
unsigned long get_time_us();
std::string get_timestamp_string();
std::string get_time_string();
float get_time_delta_ms(unsigned long start_time);

// ============================================================
// File Utilities
// ============================================================

bool file_exists(const std::string& path);
bool read_file(const std::string& path, std::string& content);
bool write_file(const std::string& path, const std::string& content);
bool create_directory(const std::string& path);
std::string get_file_name(const std::string& path);
std::string get_file_extension(const std::string& path);

// ============================================================
// String Utilities
// ============================================================

std::vector<std::string> split_string(const std::string& str, char delimiter);
std::string trim_string(const std::string& str);
std::string to_lower(const std::string& str);
std::string to_upper(const std::string& str);
bool starts_with(const std::string& str, const std::string& prefix);
bool ends_with(const std::string& str, const std::string& suffix);

// ============================================================
// Image Utilities
// ============================================================

cv::Mat resize_image(const cv::Mat& image, int width, int height);
cv::Mat resize_image_aspect(const cv::Mat& image, int max_size);
cv::Mat convert_to_grayscale(const cv::Mat& image);
cv::Mat apply_blur(const cv::Mat& image, int kernel_size);
void draw_pose(cv::Mat& image, const Pose& pose, const cv::Scalar& color = cv::Scalar(0, 255, 0));

// ============================================================
// JSON Utilities
// ============================================================

std::string json_encode(const std::string& key, const std::string& value);
std::string json_encode(const std::string& key, float value);
std::string json_encode(const std::string& key, int value);
std::string json_encode(const std::string& key, bool value);
std::string json_escape(const std::string& str);

// ============================================================
// System Utilities
// ============================================================

float get_cpu_usage();
float get_cpu_temperature();
float get_battery_voltage();
bool is_battery_low();

// ============================================================
// Logging Utilities
// ============================================================

void log_info(const std::string& module, const std::string& message);
void log_warn(const std::string& module, const std::string& message);
void log_error(const std::string& module, const std::string& message);
void log_debug(const std::string& module, const std::string& message);

#endif // UTILS_H