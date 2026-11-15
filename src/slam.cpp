#include "robot_modules.h"
#include "robot_state.h"
#include "config.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>

SLAMModule::SLAMModule() : slam_system(nullptr), initialized(false) {
    std::cout << "[SLAM] Initializing SLAM module..." << std::endl;
    const int grid_w = static_cast<int>(MAP_WIDTH / MAP_RESOLUTION);
    const int grid_h = static_cast<int>(MAP_HEIGHT / MAP_RESOLUTION);
    occupancy_grid = cv::Mat::zeros(grid_h, grid_w, CV_8U);
    occupancy_grid.setTo(127);
}

SLAMModule::~SLAMModule() {
    if (slam_system) {
        slam_system = nullptr;
    }
    std::cout << "[SLAM] SLAM module destroyed" << std::endl;
}

bool SLAMModule::init(const std::string& vocab_file, const std::string& settings_file) {
    (void)vocab_file;
    (void)settings_file;
    initialized = false;
    std::cerr << "[SLAM] ORB-SLAM is not integrated; GPS supplies navigation position"
              << std::endl;
    return false;
}

void SLAMModule::update(const cv::Mat& frame, const Pose& pose) {
    if (!initialized || frame.empty()) {
        return;
    }

    try {
        current_pose = pose;
        update_occupancy_grid(frame, pose);
        g_robot_state.x = current_pose.x;
        g_robot_state.y = current_pose.y;
        g_robot_state.theta = current_pose.theta;
    } catch (const std::exception& error) {
        std::cerr << "[SLAM] Update error: " << error.what() << std::endl;
    }
}

void SLAMModule::update_occupancy_grid(const cv::Mat& frame, const Pose& pose) {
    if (occupancy_grid.empty() || frame.empty()) {
        return;
    }

    static int frame_count = 0;
    if (++frame_count % 5 != 0) {
        return;
    }

    const int grid_w = occupancy_grid.cols;
    const int grid_h = occupancy_grid.rows;
    const float max_range = 1.5f;
    const float step = 0.1f;
    const int sample_x = std::min(frame.cols - 1, std::max(0, frame.cols / 2));
    const int sample_y = std::min(frame.rows - 1, std::max(0, frame.rows / 2));
    uchar intensity = 0;

    if (frame.channels() == 3) {
        const cv::Vec3b pixel = frame.at<cv::Vec3b>(sample_y, sample_x);
        intensity = static_cast<uchar>((pixel[0] + pixel[1] + pixel[2]) / 3);
    } else {
        intensity = frame.at<uchar>(sample_y, sample_x);
    }

    for (float distance = 0.2f; distance < max_range; distance += step) {
        const float world_x = pose.x + distance * std::cos(pose.theta);
        const float world_y = pose.y + distance * std::sin(pose.theta);
        const int map_x = static_cast<int>(
            (world_x + MAP_WIDTH * 0.5f) / MAP_RESOLUTION);
        const int map_y = static_cast<int>(
            (world_y + MAP_HEIGHT * 0.5f) / MAP_RESOLUTION);

        if (map_x < 0 || map_x >= grid_w || map_y < 0 || map_y >= grid_h) {
            continue;
        }

        occupancy_grid.at<uchar>(map_y, map_x) = intensity < 90 ? 0 : 255;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = map_x + dx;
                const int ny = map_y + dy;
                if (nx >= 0 && nx < grid_w && ny >= 0 && ny < grid_h &&
                    occupancy_grid.at<uchar>(ny, nx) == 127) {
                    occupancy_grid.at<uchar>(ny, nx) = 255;
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
    } catch (const std::exception& error) {
        std::cerr << "[SLAM] Save map error: " << error.what() << std::endl;
    }
}
