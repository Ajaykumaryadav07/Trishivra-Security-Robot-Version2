// src/slam.cpp
#include "robot_modules.h"
#include "robot_state.h"
#include "config.h"
#include <iostream>
#include <cmath>

// ============================================================
// SLAMModule Implementation
// ============================================================

SLAMModule::SLAMModule() : slam_system(nullptr), initialized(false) {
    std::cout << "[SLAM] Initializing SLAM module..." << std::endl;
    int grid_w = static_cast<int>(MAP_WIDTH / MAP_RESOLUTION);
    int grid_h = static_cast<int>(MAP_HEIGHT / MAP_RESOLUTION);
    occupancy_grid = cv::Mat::zeros(grid_h, grid_w, CV_8U);
    occupancy_grid.setTo(127);  // Unknown
}

SLAMModule::~SLAMModule() {
    if (slam_system) {
        // Cleanup ORB-SLAM3 resources when integrated.
        delete static_cast<ORB_SLAM3::System*>(slam_system);
        slam_system = nullptr;
    }
    std::cout << "[SLAM] SLAM module destroyed" << std::endl;
}

bool SLAMModule::init(const std::string& vocab_file, const std::string& settings_file) {
    try {
        // In production with ORB-SLAM3:
        // slam_system = new ORB_SLAM3::System(
        //     vocab_file,
        //     settings_file,
        //     ORB_SLAM3::System::MONOCULAR,
        //     true  // Enable viewer
        // );

        initialized = true;
        std::cout << "[SLAM] Initialized with vocab: " << vocab_file << std::endl;
        std::cout << "[SLAM] Settings: " << settings_file << std::endl;

        int grid_w = static_cast<int>(MAP_WIDTH / MAP_RESOLUTION);
        int grid_h = static_cast<int>(MAP_HEIGHT / MAP_RESOLUTION);
        occupancy_grid = cv::Mat::zeros(grid_h, grid_w, CV_8U);
        occupancy_grid.setTo(127);  // Unknown

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SLAM] Init error: " << e.what() << std::endl;
        return false;
    }
}

void SLAMModule::update(const cv::Mat& frame, const Pose& pose) {
    if (!initialized || frame.empty()) {
        return;
    }

    try {
        // In a future real SLAM integration, track monocular images here.
        // Example:
        // auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        // auto slam_pose = static_cast<ORB_SLAM3::System*>(slam_system)->TrackMonocular(frame, timestamp);
        // current_pose.x = slam_pose.translation().x();
        // current_pose.y = slam_pose.translation().y();
        // current_pose.theta = slam_pose.rotation().yaw();

        current_pose = pose;
        update_occupancy_grid(frame, pose);

        g_robot_state.x = current_pose.x;
        g_robot_state.y = current_pose.y;
        g_robot_state.theta = current_pose.theta;
    } catch (const std::exception& e) {
        std::cerr << "[SLAM] Update error: " << e.what() << std::endl;
    }
}

void SLAMModule::update_occupancy_grid(const cv::Mat& frame, const Pose& pose) {
    if (occupancy_grid.empty() || frame.empty()) {
        return;
    }

    static int frame_count = 0;
    frame_count++;
    if (frame_count % 5 != 0) {
        return;
    }

    int grid_w = occupancy_grid.cols;
    int grid_h = occupancy_grid.rows;
    float robot_theta = pose.theta;
    const float max_range = 1.5f;
    const float step = 0.1f;

    int sample_x = std::min(frame.cols - 1, std::max(0, frame.cols / 2));
    int sample_y = std::min(frame.rows - 1, std::max(0, frame.rows / 2));
    uchar intensity = 0;

    if (frame.channels() == 3) {
        const cv::Vec3b pixel = frame.at<cv::Vec3b>(sample_y, sample_x);
        intensity = static_cast<uchar>((pixel[0] + pixel[1] + pixel[2]) / 3);
    } else {
        intensity = frame.at<uchar>(sample_y, sample_x);
    }

    for (float dist = 0.2f; dist < max_range; dist += step) {
        float world_x = pose.x + dist * std::cos(robot_theta);
        float world_y = pose.y + dist * std::sin(robot_theta);

        int map_x = static_cast<int>((world_x + MAP_WIDTH * 0.5f) / MAP_RESOLUTION);
        int map_y = static_cast<int>((world_y + MAP_HEIGHT * 0.5f) / MAP_RESOLUTION);

        if (map_x < 0 || map_x >= grid_w || map_y < 0 || map_y >= grid_h) {
            continue;
        }

        if (intensity < 90) {
            occupancy_grid.at<uchar>(map_y, map_x) = 0;
        } else {
            occupancy_grid.at<uchar>(map_y, map_x) = 255;
        }

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int nx = map_x + dx;
                int ny = map_y + dy;
                if (nx >= 0 && nx < grid_w && ny >= 0 && ny < grid_h) {
                    if (occupancy_grid.at<uchar>(ny, nx) == 127) {
                        occupancy_grid.at<uchar>(ny, nx) = 255;
                    }
                }
            }
        }
    }
}

Pose SLAMModule::get_pose() const {
    return current_pose;
}

cv::Mat SLAMModule::get_map() const {
    return occupancy_grid.clone();
}

void SLAMModule::save_map(const std::string& path) {
    if (occupancy_grid.empty()) {
        std::cerr << "[SLAM] No map to save" << std::endl;
        return;
    }

    try {
        cv::Mat map_to_save;
        occupancy_grid.convertTo(map_to_save, CV_8U);
        cv::imwrite(path, map_to_save);
        std::cout << "[SLAM] Map saved to: " << path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[SLAM] Save map error: " << e.what() << std::endl;
    }
}

// ============================================================
// Optional SLAM helpers (disabled until real SLAM is integrated)
// ============================================================

/*
std::vector<cv::Point2f> SLAMModule::track_features(const cv::Mat& prev_frame,
                                                     const cv::Mat& curr_frame,
                                                     std::vector<cv::Point2f>& prev_points) {
    std::vector<cv::Point2f> curr_points;
    if (prev_frame.empty() || curr_frame.empty() || prev_points.empty()) {
        return curr_points;
    }

    try {
        std::vector<uchar> status;
        std::vector<float> err;
        cv::calcOpticalFlowPyrLK(prev_frame, curr_frame,
                                  prev_points, curr_points,
                                  status, err);

        std::vector<cv::Point2f> valid_curr;
        for (size_t i = 0; i < status.size(); i++) {
            if (status[i]) {
                valid_curr.push_back(curr_points[i]);
            }
        }

        prev_points = valid_curr;
        return curr_points;
    } catch (const std::exception& e) {
        std::cerr << "[SLAM] Feature tracking error: " << e.what() << std::endl;
        return curr_points;
    }
}
*/

/*
bool SLAMModule::merge_map(const cv::Mat& new_map, const Pose& offset) {
    if (new_map.empty() || occupancy_grid.empty()) {
        return false;
    }

    try {
        cv::Mat aligned_map;
        cv::warpAffine(new_map, aligned_map,
                       cv::getRotationMatrix2D(cv::Point2f(0, 0), 0.0, 1.0),
                       occupancy_grid.size());

        cv::Mat merged;
        cv::addWeighted(occupancy_grid, 0.5, aligned_map, 0.5, 0.0, merged);
        occupancy_grid = merged;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SLAM] Map merge error: " << e.what() << std::endl;
        return false;
    }
}
*/

/*
bool SLAMModule::detect_loop_closure(const std::vector<cv::KeyPoint>& current_kp,
                                      const cv::Mat& current_desc,
                                      std::vector<cv::KeyPoint>& prev_kp,
                                      cv::Mat& prev_desc) {
    if (current_kp.empty() || current_desc.empty()) {
        return false;
    }

    try {
        cv::BFMatcher matcher(cv::NORM_HAMMING);
        std::vector<cv::DMatch> matches;
        matcher.match(prev_desc, current_desc, matches);

        if (matches.size() < 20) {
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SLAM] Loop closure error: " << e.what() << std::endl;
        return false;
    }
}
*/
