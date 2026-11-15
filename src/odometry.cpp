#include "robot_modules.h"
#include "robot_state.h"
#include "config.h"
#include <cmath>
#include <iostream>
#include <mutex>

// ============================================================
// Odometry Class Implementation
// ============================================================

class Odometry {
public:
    Odometry();
    ~Odometry();
    
    // Update odometry with new encoder and IMU data
    void update(int left_ticks, int right_ticks, float gyro_z, float dt);
    
    // Get current pose
    Pose get_pose() const;
    
    // Reset odometry to origin
    void reset();
    
    // Set wheel parameters
    void set_wheel_params(float wheel_diameter, float wheel_base, int ticks_per_revolution);
    
    // Get individual components
    float get_left_distance() const { return left_distance; }
    float get_right_distance() const { return right_distance; }
    float get_heading() const { return theta; }
    
private:
    // Robot parameters
    float wheel_diameter;
    float wheel_base;
    float ticks_per_revolution;
    float distance_per_tick;
    
    // State
    float x;          // X position (meters)
    float y;          // Y position (meters)
    float theta;      // Heading (radians)
    
    // Previous readings
    int prev_left_ticks;
    int prev_right_ticks;
    float prev_gyro_z;
    
    // Distance traveled
    float left_distance;
    float right_distance;
    float total_distance;
    
    // Mutex for thread safety
    mutable std::mutex mutex;
    
    // Internal helper functions
    float ticks_to_distance(int ticks);
    void calculate_pose(float left_dist, float right_dist, float gyro_z, float dt);
    float fuse_odometry(float enc_theta, float gyro_theta, float dt);
    float normalize_angle(float angle);
};

// ============================================================
// Constructor
// ============================================================

Odometry::Odometry() 
    : wheel_diameter(WHEEL_DIAMETER_CM / 100.0f),  // Convert cm to meters
      wheel_base(WHEEL_BASE_CM / 100.0f),          // Convert cm to meters
      ticks_per_revolution(ENCODER_PPR),
      distance_per_tick(0.0f),
      x(0.0f), y(0.0f), theta(0.0f),
      prev_left_ticks(0), prev_right_ticks(0), prev_gyro_z(0.0f),
      left_distance(0.0f), right_distance(0.0f), total_distance(0.0f) {
    
    // Calculate distance per tick
    // Distance per tick = (π * wheel_diameter) / ticks_per_revolution
    distance_per_tick = (M_PI * wheel_diameter) / ticks_per_revolution;
    
    std::cout << "[ODOMETRY] Initialized" << std::endl;
    std::cout << "[ODOMETRY] Wheel diameter: " << wheel_diameter << " m" << std::endl;
    std::cout << "[ODOMETRY] Wheel base: " << wheel_base << " m" << std::endl;
    std::cout << "[ODOMETRY] Ticks per revolution: " << ticks_per_revolution << std::endl;
    std::cout << "[ODOMETRY] Distance per tick: " << distance_per_tick << " m" << std::endl;
}

// ============================================================
// Destructor
// ============================================================

Odometry::~Odometry() {
    std::cout << "[ODOMETRY] Destroyed" << std::endl;
}

// ============================================================
// Update Odometry
// ============================================================

void Odometry::update(int left_ticks, int right_ticks, float gyro_z, float dt) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Calculate distance traveled from encoders
    int left_delta = left_ticks - prev_left_ticks;
    int right_delta = right_ticks - prev_right_ticks;
    
    float left_dist = ticks_to_distance(left_delta);
    float right_dist = ticks_to_distance(right_delta);
    
    // Update individual distances
    left_distance += left_dist;
    right_distance += right_dist;
    total_distance += (left_dist + right_dist) / 2.0f;
    
    // Calculate pose using encoder data
    calculate_pose(left_dist, right_dist, gyro_z, dt);
    
    // Store current values for next iteration
    prev_left_ticks = left_ticks;
    prev_right_ticks = right_ticks;
    prev_gyro_z = gyro_z;
    
    // Update global state
    g_robot_state.x = x;
    g_robot_state.y = y;
    g_robot_state.theta = theta;
}

// ============================================================
// Calculate Pose
// ============================================================

void Odometry::calculate_pose(float left_dist, float right_dist, float gyro_z, float dt) {
    // ===== Method 1: Encoder-based odometry =====
    float center_dist = (left_dist + right_dist) / 2.0f;
    float enc_theta = (right_dist - left_dist) / wheel_base;
    
    // ===== Method 2: Gyro-based heading =====
    float gyro_theta = prev_gyro_z + gyro_z * dt;
    gyro_theta = normalize_angle(gyro_theta);
    
    // ===== Sensor Fusion: Combine encoder and gyro =====
    // Use gyro for heading correction (more reliable for orientation)
    float fused_theta = fuse_odometry(enc_theta, gyro_theta, dt);
    
    // Update position using encoder distance and fused heading
    x += center_dist * cos(fused_theta);
    y += center_dist * sin(fused_theta);
    theta = fused_theta;
    
    // Keep theta in range [-PI, PI]
    theta = normalize_angle(theta);
}

// ============================================================
// Sensor Fusion (Encoder + Gyro)
// ============================================================

float Odometry::fuse_odometry(float enc_theta, float gyro_theta, float dt) {
    // Simple complementary filter
    // alpha = 0.98 (gyro) + 0.02 (encoder)
    // Gyro is more reliable for short-term orientation
    // Encoder is more reliable for long-term (no drift)
    
    float alpha = 0.98f;  // Gyro weight
    
    // Calculate heading difference
    float theta_diff = gyro_theta - enc_theta;
    theta_diff = normalize_angle(theta_diff);
    
    // Complementary filter
    float fused_theta = alpha * gyro_theta + (1.0f - alpha) * enc_theta;
    
    return normalize_angle(fused_theta);
}

// ============================================================
// Helper Functions
// ============================================================

float Odometry::ticks_to_distance(int ticks) {
    return ticks * distance_per_tick;
}

float Odometry::normalize_angle(float angle) {
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
    return angle;
}

// ============================================================
// Get Pose
// ============================================================

Pose Odometry::get_pose() const {
    std::lock_guard<std::mutex> lock(mutex);
    Pose pose;
    pose.x = x;
    pose.y = y;
    pose.theta = theta;
    return pose;
}

// ============================================================
// Reset Odometry
// ============================================================

void Odometry::reset() {
    std::lock_guard<std::mutex> lock(mutex);
    x = 0.0f;
    y = 0.0f;
    theta = 0.0f;
    left_distance = 0.0f;
    right_distance = 0.0f;
    total_distance = 0.0f;
    prev_left_ticks = 0;
    prev_right_ticks = 0;
    prev_gyro_z = 0.0f;
    std::cout << "[ODOMETRY] Reset to origin" << std::endl;
}

// ============================================================
// Set Wheel Parameters
// ============================================================

void Odometry::set_wheel_params(float wheel_diameter, float wheel_base, int ticks_per_revolution) {
    std::lock_guard<std::mutex> lock(mutex);
    
    this->wheel_diameter = wheel_diameter;
    this->wheel_base = wheel_base;
    this->ticks_per_revolution = ticks_per_revolution;
    
    // Recalculate distance per tick
    distance_per_tick = (M_PI * wheel_diameter) / ticks_per_revolution;
    
    std::cout << "[ODOMETRY] Wheel parameters updated" << std::endl;
}

// ============================================================
// Global Instance
// ============================================================

static Odometry g_odometry;

// ============================================================
// Global Functions (for use in main.cpp)
// ============================================================

void odometry_init(float wheel_diameter_cm = WHEEL_DIAMETER_CM, 
                   float wheel_base_cm = WHEEL_BASE_CM, 
                   int ticks_per_rev = ENCODER_PPR) {
    g_odometry.set_wheel_params(
        wheel_diameter_cm / 100.0f, 
        wheel_base_cm / 100.0f, 
        ticks_per_rev
    );
    g_odometry.reset();
}

void odometry_update(int left_ticks, int right_ticks, float gyro_z, float dt) {
    g_odometry.update(left_ticks, right_ticks, gyro_z, dt);
}

Pose odometry_get_pose() {
    return g_odometry.get_pose();
}

void odometry_reset() {
    g_odometry.reset();
}

float odometry_get_left_distance() {
    return g_odometry.get_left_distance();
}

float odometry_get_right_distance() {
    return g_odometry.get_right_distance();
}

float odometry_get_heading() {
    return g_odometry.get_heading();
}