#include "robot_modules.h"
#include "config.h"
#include <iostream>
#include <limits>

// ============================================================
// DepthEstimator Implementation
// ============================================================

DepthEstimator::DepthEstimator() : use_midas(false), model(nullptr), initialized(false) {
    std::cout << "[DEPTH] Disabled: no calibrated metric depth model is configured"
              << std::endl;
}

DepthEstimator::~DepthEstimator() {
    // Cleanup model if loaded
    if (model) {
        // Free MiDaS model resources
        model = nullptr;
    }
    std::cout << "[DEPTH] Depth estimator destroyed" << std::endl;
}

cv::Mat DepthEstimator::estimate(const cv::Mat& frame) {
    (void)frame;
    return cv::Mat();
}

float DepthEstimator::get_distance(const cv::Point& pixel) {
    (void)pixel;
    return std::numeric_limits<float>::quiet_NaN();
}

// ============================================================
// Advanced MiDaS Integration (Optional)
// ============================================================

/*
void DepthEstimator::load_midas_model(const std::string& model_path) {
    try {
        // Load MiDaS model
        // This requires OpenCV DNN module and MiDaS model file
        
        cv::dnn::Net net = cv::dnn::readNetFromTorch(model_path);
        if (net.empty()) {
            std::cerr << "[DEPTH] Failed to load MiDaS model" << std::endl;
            return;
        }
        
        model = new cv::dnn::Net(net);
        use_midas = true;
        initialized = true;
        
        std::cout << "[DEPTH] MiDaS model loaded: " << model_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DEPTH] MiDaS load error: " << e.what() << std::endl;
        use_midas = false;
    }
}

cv::Mat DepthEstimator::estimate_midas(const cv::Mat& frame) {
    if (!model || frame.empty()) {
        return cv::Mat();
    }
    
    try {
        cv::dnn::Net* net = static_cast<cv::dnn::Net*>(model);
        
        // Preprocess
        cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, 
                                               cv::Size(256, 256), 
                                               cv::Scalar(), true, false);
        
        // Inference
        net->setInput(blob);
        cv::Mat output = net->forward();
        
        // Postprocess
        cv::Mat depth_map;
        output = output.reshape(1, 256);
        cv::resize(output, depth_map, frame.size());
        
        // Normalize
        depth_map.convertTo(depth_map, CV_32F);
        cv::normalize(depth_map, depth_map, 0, 1, cv::NORM_MINMAX);
        
        return depth_map;
    } catch (const std::exception& e) {
        std::cerr << "[DEPTH] MiDaS inference error: " << e.what() << std::endl;
        return cv::Mat();
    }
}
*/

// ============================================================
// Depth to LaserScan Conversion (for obstacle avoidance)
// ============================================================

/*
cv::Mat DepthEstimator::depth_to_occupancy(const cv::Mat& depth_map, 
                                           const Pose& pose,
                                           float threshold) {
    if (depth_map.empty()) {
        return cv::Mat();
    }
    
    // Convert depth map to occupancy grid
    int rows = depth_map.rows;
    int cols = depth_map.cols;
    
    cv::Mat occupancy = cv::Mat::zeros(rows, cols, CV_8U);
    
    for (int y = 0; y < rows; y += 2) {
        for (int x = 0; x < cols; x += 2) {
            float depth = depth_map.at<float>(y, x);
            
            // If depth is less than threshold, mark as occupied
            if (depth > 0 && depth < threshold) {
                occupancy.at<uchar>(y, x) = 255;
                
                // Also mark surrounding cells
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        int ny = y + dy;
                        int nx = x + dx;
                        if (ny >= 0 && ny < rows && nx >= 0 && nx < cols) {
                            occupancy.at<uchar>(ny, nx) = 255;
                        }
                    }
                }
            }
        }
    }
    
    return occupancy;
}
*/

// ============================================================
// Stereo Depth (if using two cameras)
// ============================================================

/*
cv::Mat DepthEstimator::estimate_stereo(const cv::Mat& left, 
                                        const cv::Mat& right) {
    if (left.empty() || right.empty()) {
        return cv::Mat();
    }
    
    try {
        cv::Mat gray_left, gray_right;
        cv::cvtColor(left, gray_left, cv::COLOR_BGR2GRAY);
        cv::cvtColor(right, gray_right, cv::COLOR_BGR2GRAY);
        
        // Stereo BM
        cv::Ptr<cv::StereoBM> stereo = cv::StereoBM::create(16, 15);
        cv::Mat disparity;
        stereo->compute(gray_left, gray_right, disparity);
        
        // Convert to depth
        cv::Mat depth_map;
        disparity.convertTo(depth_map, CV_32F, 1.0 / 16.0);
        
        // Focal length * baseline / disparity
        float f = 600.0f;  // Focal length in pixels
        float b = 0.1f;    // Baseline in meters
        
        for (int y = 0; y < depth_map.rows; y++) {
            for (int x = 0; x < depth_map.cols; x++) {
                float d = depth_map.at<float>(y, x);
                if (d > 0.1f) {
                    depth_map.at<float>(y, x) = (f * b) / d;
                } else {
                    depth_map.at<float>(y, x) = 5.0f;  // Max distance
                }
            }
        }
        
        return depth_map;
    } catch (const std::exception& e) {
        std::cerr << "[DEPTH] Stereo error: " << e.what() << std::endl;
        return cv::Mat();
    }
}
*/