#include "robot_modules.h"
#include "config.h"
#include "robot_state.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <cstdlib>

// ============================================================
// CameraModule Implementation
// ============================================================

CameraModule::CameraModule() : initialized(false), fps(CAMERA_FPS), camera_type(CameraType::UNKNOWN),
                               night_vision_enabled(false), night_mode(false), 
                               auto_brightness_enabled(true), average_brightness(0.0f),
                               ir_led_gpio_pin(17) {
    std::cout << "[CAMERA] Camera module created (with Night Vision support)" << std::endl;
}

CameraModule::~CameraModule() {
    release();
    std::cout << "[CAMERA] Camera module destroyed" << std::endl;
}

// ============================================================
// Get Camera Type Name
// ============================================================

std::string CameraModule::get_camera_type_name() const {
    switch (camera_type) {
        case CameraType::PI_CAMERA:
            return "Pi Camera Module (libcamera)";
        case CameraType::UNKNOWN:
        default:
            return "Unknown Camera";
    }
}

// ============================================================
// Detect Pi Camera Module
// ============================================================

bool CameraModule::detect_pi_camera() {
    std::ifstream dt_file("/proc/device-tree/model");
    if (dt_file.is_open()) {
        std::string model;
        std::getline(dt_file, model);
        dt_file.close();
        
        if (model.find("Raspberry Pi") != std::string::npos) {
            camera_type = CameraType::PI_CAMERA;
            std::cout << "[CAMERA] Raspberry Pi detected: " << model << std::endl;
            std::cout << "[CAMERA] Will open CSI camera through libcamera/GStreamer" << std::endl;
            return true;
        }
    }
    
    camera_type = CameraType::UNKNOWN;
    return false;
}

// ============================================================
// Initialize Camera
// ============================================================

bool CameraModule::init(int width, int height, int fps) {
    if (initialized) {
        release();
    }
    
    this->fps = fps;
    
    try {
        if (!detect_pi_camera()) {
            std::cerr << "[CAMERA] ❌ Raspberry Pi camera is only supported camera" << std::endl;
            return false;
        }

        std::cout << "[CAMERA] Camera type: " << get_camera_type_name() << std::endl;

        const std::string pipeline =
            "libcamerasrc ! video/x-raw,width=" + std::to_string(width) +
            ",height=" + std::to_string(height) + ",framerate=" +
            std::to_string(fps) +
            "/1 ! videoconvert ! video/x-raw,format=BGR ! "
            "appsink drop=true max-buffers=1";
        if (!cap.open(pipeline, cv::CAP_GSTREAMER)) {
            std::cerr << "[CAMERA] ❌ Could not open CSI camera with libcamera/GStreamer. "
                         "Check the camera connection and GStreamer libcamera plugin."
                      << std::endl;
            return false;
        }
        
        if (!cap.isOpened()) {
            std::cerr << "[CAMERA] ❌ Failed to open camera!" << std::endl;
            return false;
        }

        cv::Mat test_frame;
        if (!cap.read(test_frame) || test_frame.empty()) {
            std::cerr << "[CAMERA] ❌ Opened camera but could not capture a frame!" << std::endl;
            cap.release();
            camera_type = CameraType::UNKNOWN;
            return false;
        }
        
        // Verify actual settings
        int actual_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int actual_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        double actual_fps = cap.get(cv::CAP_PROP_FPS);
        
        initialized = true;
        
        std::cout << "[CAMERA] ✅ Initialized: " << actual_width << "x" << actual_height 
                  << " @ " << actual_fps << " FPS (requested: " << fps << " FPS)" << std::endl;
        std::cout << "[CAMERA] Type: " << get_camera_type_name() << std::endl;
        
        // Check if FPS matches request
        if (static_cast<int>(actual_fps) != fps && actual_fps > 0) {
            std::cout << "[CAMERA] ⚠️  Camera FPS (" << actual_fps 
                      << ") != requested (" << fps << ")" << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA] ❌ Init error: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================
// Capture Frame
// ============================================================

bool CameraModule::capture(cv::Mat& frame) {
    if (!initialized) {
        std::cerr << "[CAMERA] ❌ Camera not initialized!" << std::endl;
        return false;
    }
    
    try {
        // Capture frame
        if (!cap.read(frame)) {
            std::cerr << "[CAMERA] ❌ Failed to capture frame!" << std::endl;
            return false;
        }
        
        if (frame.empty()) {
            std::cerr << "[CAMERA] ❌ Empty frame captured!" << std::endl;
            return false;
        }
        
        // Auto-detect brightness for night vision
        if (night_vision_enabled) {
            detect_brightness(frame);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA] ❌ Capture error: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================
// Release Camera
// ============================================================

void CameraModule::release() {
    if (cap.isOpened()) {
        cap.release();
        std::cout << "[CAMERA] Released" << std::endl;
    }
    initialized = false;
    camera_type = CameraType::UNKNOWN;
    night_mode = false;

    // Turn off IR LED if supported
    try {
        set_ir_led(false);
    } catch (...) {
        // Ignore if GPIO not available
    }
}

// ============================================================
// Night Vision: Calculate Average Brightness
// ============================================================

float CameraModule::calculate_average_brightness(const cv::Mat& frame) {
    if (frame.empty()) return 0.0f;
    
    // Convert to grayscale for brightness calculation
    cv::Mat gray;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame.clone();
    }
    
    // Calculate mean brightness
    cv::Scalar mean = cv::mean(gray);
    return static_cast<float>(mean[0]);
}

// ============================================================
// Night Vision: Detect Brightness and Update Mode
// ============================================================

void CameraModule::detect_brightness(const cv::Mat& frame) {
    if (!night_vision_enabled || frame.empty()) return;
    
    average_brightness = calculate_average_brightness(frame);
    
    if (auto_brightness_enabled) {
        update_night_mode();
    }
}

// ============================================================
// Night Vision: Update Night Mode Based on Brightness
// ============================================================

void CameraModule::update_night_mode() {
    // Threshold for dark/light detection (0-255 scale)
    const float DARK_THRESHOLD = 40.0f;
    const float BRIGHT_THRESHOLD = 60.0f;
    
    bool should_be_night = (average_brightness < DARK_THRESHOLD);
    
    if (should_be_night && !night_mode) {
        // Switch to night mode
        set_night_mode(true);
    } else if (!should_be_night && night_mode) {
        // Switch to day mode
        set_night_mode(false);
    }
}

// ============================================================
// Night Vision: Enable/Disable
// ============================================================

void CameraModule::enable_night_vision(bool enable) {
    night_vision_enabled = enable;
    if (!enable) {
        set_night_mode(false);
        set_ir_led(false);
    }
    std::cout << "[CAMERA] Night vision " << (enable ? "ENABLED" : "DISABLED") << std::endl;
}

// ============================================================
// Night Vision: Set Night Mode
// ============================================================

void CameraModule::set_night_mode(bool enable) {
    if (night_mode == enable) return;
    
    night_mode = enable;
    
    if (!initialized) return;
    
    if (night_mode) {
        std::cout << "[CAMERA] 🌙 NIGHT MODE activated (Brightness: " 
                  << average_brightness << ")" << std::endl;
        
        // Turn on IR LED
        set_ir_led(true);
        
        // Adjust camera settings for IR/night mode
        try {
            // Increase exposure/gain for night
            cap.set(cv::CAP_PROP_BRIGHTNESS, 50);
            cap.set(cv::CAP_PROP_GAIN, 100);
            cap.set(cv::CAP_PROP_SATURATION, 0);  // Reduce color saturation for IR
            
            // Try to disable auto white balance for consistent IR
            cap.set(cv::CAP_PROP_AUTO_WB, 0);
        } catch (...) {
            // Properties may not be available on all cameras
        }
    } else {
        std::cout << "[CAMERA] ☀️  DAY MODE activated (Brightness: " 
                  << average_brightness << ")" << std::endl;
        
        // Turn off IR LED
        set_ir_led(false);
        
        // Reset to normal daytime settings
        try {
            cap.set(cv::CAP_PROP_BRIGHTNESS, 0);
            cap.set(cv::CAP_PROP_GAIN, 50);
            cap.set(cv::CAP_PROP_SATURATION, 64);
            cap.set(cv::CAP_PROP_AUTO_WB, 1);  // Re-enable auto white balance
        } catch (...) {
            // Properties may not be available
        }
    }
}

// ============================================================
// Night Vision: IR LED Control
// ============================================================

void CameraModule::set_ir_led(bool on) {
    // GPIO control for IR LED (GPIO 17 or 18 on Raspberry Pi)
    // This requires wiringPi or similar GPIO library
    
    try {
        // Simple file-based GPIO control (works on Pi)
        std::ofstream gpio_file("/sys/class/gpio/gpio17/value");
        if (gpio_file.is_open()) {
            gpio_file << (on ? "1" : "0");
            gpio_file.close();
            
            std::cout << "[CAMERA] IR LED: " << (on ? "ON 🔴" : "OFF ⚫") << std::endl;
            return;
        }
    } catch (...) {
        // GPIO control not available, continue without IR
    }
    
    if (!on) {
        std::cout << "[CAMERA] IR LED control unavailable (GPIO not accessible)" << std::endl;
    }
}

// ============================================================
// Night Vision: Set Exposure
// ============================================================

void CameraModule::set_exposure(int exposure_value) {
    // exposure_value: -1 (darker) to 1 (brighter)
    if (!initialized) return;
    
    try {
        // Map to camera-specific exposure range (typically -13 to +13)
        int camera_exposure = exposure_value * 13;
        cap.set(cv::CAP_PROP_EXPOSURE, camera_exposure);
        
        std::cout << "[CAMERA] Exposure set to " << exposure_value << std::endl;
    } catch (...) {
        std::cerr << "[CAMERA] ⚠️  Cannot set exposure (not supported)" << std::endl;
    }
}

// ============================================================
// Night Vision: Set Gain
// ============================================================

void CameraModule::set_gain(float gain_value) {
    // gain_value: 0.0 (min) to 2.0 (max)
    if (!initialized) return;
    
    if (gain_value < 0.0f || gain_value > 2.0f) {
        std::cerr << "[CAMERA] ⚠️  Gain must be between 0.0 and 2.0" << std::endl;
        return;
    }
    
    try {
        // Map to 0-100 range
        int camera_gain = static_cast<int>(gain_value * 50);
        cap.set(cv::CAP_PROP_GAIN, camera_gain);
        
        std::cout << "[CAMERA] Gain set to " << gain_value << std::endl;
    } catch (...) {
        std::cerr << "[CAMERA] ⚠️  Cannot set gain (not supported)" << std::endl;
    }
}

// ============================================================
// Optional: Set Camera Properties
// ============================================================

/*
bool CameraModule::set_property(int prop_id, double value) {
    if (!initialized) {
        return false;
    }
    
    try {
        return cap.set(prop_id, value);
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA] Set property error: " << e.what() << std::endl;
        return false;
    }
}

double CameraModule::get_property(int prop_id) {
    if (!initialized) {
        return -1.0;
    }
    
    try {
        return cap.get(prop_id);
    } catch (const std::exception& e) {
        std::cerr << "[CAMERA] Get property error: " << e.what() << std::endl;
        return -1.0;
    }
}
*/

// ============================================================
// Optional: Get Camera Info
// ============================================================

/*
std::string CameraModule::get_camera_info() {
    if (!initialized) {
        return "Camera not initialized";
    }
    
    std::string info = "Camera Info:\n";
    info += "  Width: " + std::to_string(static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH))) + "\n";
    info += "  Height: " + std::to_string(static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT))) + "\n";
    info += "  FPS: " + std::to_string(cap.get(cv::CAP_PROP_FPS)) + "\n";
    info += "  Format: " + std::to_string(static_cast<int>(cap.get(cv::CAP_PROP_FORMAT))) + "\n";
    
    return info;
}
*/

// ============================================================
// Optional: Capture with Timestamp
// ============================================================

/*
bool CameraModule::capture_with_timestamp(cv::Mat& frame, long long& timestamp_ms) {
    if (!capture(frame)) {
        return false;
    }
    
    timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    
    return true;
}
*/

// ============================================================
// Optional: Resize Frame (for YOLO)
// ============================================================

/*
cv::Mat CameraModule::resize_for_yolo(const cv::Mat& frame, int target_size) {
    if (frame.empty()) {
        return cv::Mat();
    }
    
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(target_size, target_size));
    return resized;
}
*/

// ============================================================
// Optional: Convert to Grayscale
// ============================================================

/*
cv::Mat CameraModule::to_grayscale(const cv::Mat& frame) {
    if (frame.empty()) {
        return cv::Mat();
    }
    
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    return gray;
}
*/

// ============================================================
// Optional: Apply Image Preprocessing
// ============================================================

/*
cv::Mat CameraModule::preprocess_frame(const cv::Mat& frame) {
    if (frame.empty()) {
        return cv::Mat();
    }
    
    cv::Mat processed = frame.clone();
    
    // Auto white balance (simplified)
    // cv::cvtColor(processed, processed, cv::COLOR_BGR2LAB);
    // 
    // // Apply histogram equalization to L channel
    // std::vector<cv::Mat> lab_channels;
    // cv::split(processed, lab_channels);
    // cv::equalizeHist(lab_channels[0], lab_channels[0]);
    // cv::merge(lab_channels, processed);
    // cv::cvtColor(processed, processed, cv::COLOR_LAB2BGR);
    
    // Denoise (optional)
    // cv::fastNlMeansDenoisingColored(processed, processed, 10, 10, 7, 21);
    
    return processed;
}
*/

// ============================================================
// Optional: Pi Camera Module 2 NoIR Specific
// ============================================================

/*
#ifdef USE_PI_CAMERA
#include <raspicam/raspicam_cv.h>

class PiCameraWrapper {
public:
    raspicam::RaspiCam_Cv camera;
    
    bool init(int width, int height, int fps) {
        camera.set(cv::CAP_PROP_FRAME_WIDTH, width);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        camera.set(cv::CAP_PROP_FPS, fps);
        camera.set(cv::CAP_PROP_BRIGHTNESS, 50);
        camera.set(cv::CAP_PROP_CONTRAST, 50);
        camera.set(cv::CAP_PROP_SATURATION, 50);
        
        return camera.open();
    }
    
    bool capture(cv::Mat& frame) {
        return camera.grab() && camera.retrieve(frame);
    }
    
    void release() {
        camera.release();
    }
};
#endif
*/