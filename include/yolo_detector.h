#ifndef YOLO_DETECTOR_H
#define YOLO_DETECTOR_H

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "robot_modules.h"

// ============================================================
// YOLO Detector Class
// ============================================================

class YOLODetector {
public:
    YOLODetector();
    ~YOLODetector();

    bool init(const std::string& model_path, float threshold = 0.5f);
    std::vector<Detection> detect(const cv::Mat& frame);
    bool is_initialized() const { return initialized; }

private:
    void* model;
    float confidence_threshold;
    int img_size;
    bool initialized;
};

#endif // YOLO_DETECTOR_H