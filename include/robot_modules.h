#ifndef ROBOT_MODULES_H
#define ROBOT_MODULES_H

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

// ===== DETECTION STRUCT =====
struct Detection {
    std::string class_name;
    float confidence;
    cv::Rect bbox;
};

// ===== POSE STRUCT =====
struct Pose {
    float x = 0.0f;
    float y = 0.0f;
    float theta = 0.0f;
};

// ===== VELOCITY COMMAND =====
struct VelocityCmd {
    float linear = 0.0f;
    float angular = 0.0f;
};

// ===== CAMERA MODULE =====
enum class CameraType {
    UNKNOWN,
    PI_CAMERA,
    USB_CAMERA,
    V4L2_DEVICE
};

class CameraModule {
public:
    CameraModule();
    ~CameraModule();
    bool init(int width = 640, int height = 480, int fps = 6);
    bool capture(cv::Mat& frame);
    void release();
    bool is_initialized() const { return initialized; }
    CameraType get_camera_type() const { return camera_type; }
    std::string get_camera_type_name() const;
    
    // Night Vision Features
    void enable_night_vision(bool enable);
    void set_night_mode(bool enable);
    void set_ir_led(bool on);
    bool is_night_mode() const { return night_mode; }
    float get_brightness() const { return average_brightness; }
    void enable_auto_brightness(bool enable) { auto_brightness_enabled = enable; }
    void set_exposure(int exposure_value);  // -1 to 1 (relative)
    void set_gain(float gain_value);  // 0.0 to 2.0
    
private:
    cv::VideoCapture cap;
    bool initialized;
    int fps;
    CameraType camera_type;
    
    // Night Vision
    bool night_vision_enabled;
    bool night_mode;
    bool auto_brightness_enabled;
    float average_brightness;
    int ir_led_gpio_pin;
    
    // Camera detection methods
    bool detect_pi_camera();
    bool detect_usb_camera(int start_index = 0);
    CameraType identify_camera_type(int index);
    
    // Night vision helper methods
    void detect_brightness(const cv::Mat& frame);
    float calculate_average_brightness(const cv::Mat& frame);
    void update_night_mode();
};

// ===== YOLO DETECTOR =====
// Forward declaration for modules that include robot_modules.h.
// The full definition is available from yolo_detector.h.
class YOLODetector;

// ===== MOTOR CONTROLLER =====
class MotorController {
public:
    MotorController();
    ~MotorController();
    bool init(const std::string& port = "/dev/ttyUSB0", int baud = 115200);
    void set_speed(float left, float right);  // -1.0 to 1.0
    void stop();
    void send_command(char cmd);
    void set_steering_angle(int angle);  // 0-180


     // 🔽 ADD THESE LINES 🔽
    void send_virtual_obstacle_left();
    void send_virtual_obstacle_right();
    void request_status();
    void flush_buffer();
    // 🔼 ADD THESE LINES 🔼

    bool is_avoiding() const;
    std::string get_last_message() const;
    bool is_connected() const { return connected; }
    
private:
    void* serial;
    bool connected;
    bool avoiding;
    std::string last_message;
    void read_thread_func();
    void process_response(const std::string& response);
};

// ===== SLAM MODULE =====
class SLAMModule {
public:
    SLAMModule();
    ~SLAMModule();
    bool init(const std::string& vocab_file, const std::string& settings_file);
    void update(const cv::Mat& frame, const Pose& pose);
    Pose get_pose() const;
    cv::Mat get_map() const;
    void save_map(const std::string& path);
    bool is_initialized() const { return initialized; }
    
private:
    void* slam_system;
    bool initialized;
    Pose current_pose;
    cv::Mat occupancy_grid;
    void update_occupancy_grid(const cv::Mat& frame, const Pose& pose);
};

// ===== NAVIGATION MODULE =====
class NavigationModule {
public:
    NavigationModule();
    ~NavigationModule();
    void set_waypoints(const std::vector<Pose>& waypoints);
    void set_goal(const Pose& goal);
    void update_pose(const Pose& pose);
    Pose get_current_pose() const;
    std::vector<Pose> plan_path();
    VelocityCmd get_velocity();
    void clear_path();
    void set_map(const cv::Mat& map);
    bool is_goal_reached() const;
    int get_current_waypoint_index() const { return current_waypoint_index; }
    
private:
    std::vector<Pose> waypoints;
    int current_waypoint_index;
    Pose current_pose;
    Pose goal;
    cv::Mat map;
    std::vector<Pose> path;
    
    std::vector<Pose> a_star(const Pose& start, const Pose& goal);
    float normalize_angle(float angle);
};

// ===== DEPTH ESTIMATOR =====
class DepthEstimator {
public:
    DepthEstimator();
    ~DepthEstimator();
    cv::Mat estimate(const cv::Mat& frame);
    float get_distance(const cv::Point& pixel);
    bool is_initialized() const { return initialized; }
    
private:
    bool use_midas;
    void* model;
    bool initialized;
};

#endif // ROBOT_MODULES_H