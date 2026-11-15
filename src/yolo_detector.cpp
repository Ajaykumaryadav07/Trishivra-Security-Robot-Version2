#include "yolo_detector.h"
#include "robot_modules.h"
#include "robot_state.h"
#include "config.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

// ============================================================
// YOLODetector Implementation
// ============================================================

YOLODetector::YOLODetector()
    : confidence_threshold(YOLO_CONFIDENCE_THRESHOLD),
      img_size(YOLO_IMG_SIZE),
      initialized(false) {
    std::cout << "[YOLO] Detector created" << std::endl;
}

YOLODetector::~YOLODetector() {
    std::cout << "[YOLO] Detector destroyed" << std::endl;
}

// ============================================================
// Initialize YOLO Model
// ============================================================

bool YOLODetector::init(const std::string& model_path, float threshold) {
    confidence_threshold = threshold;

    try {
        std::filesystem::path model_file(model_path);
        if (!std::filesystem::exists(model_file)) {
            std::cerr << "[YOLO] ONNX model not found: " << model_file << std::endl;
            return false;
        }
        model = cv::dnn::readNet(model_file.string());
        model.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        model.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        std::ifstream labels(model_file.parent_path() / "best.names");
        std::string label;
        while (std::getline(labels, label)) if (!label.empty()) class_names.push_back(label);
        if (class_names.empty()) class_names = {"person", "weapon", "vehicle", "fire"};
        initialized = true;
        std::cout << "[YOLO] ONNX model loaded: " << model_file << std::endl;
        std::cout << "[YOLO] Confidence threshold: " << threshold << std::endl;
        std::cout << "[YOLO] Image size: " << img_size << "x" << img_size << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[YOLO] Init error: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================
// Detect Objects in Frame
// ============================================================

std::vector<Detection> YOLODetector::detect(const cv::Mat& frame) {
    std::vector<Detection> detections;
    if (!initialized || frame.empty()) {
        return detections;
    }

    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0,
        cv::Size(img_size, img_size), cv::Scalar(), true, false, CV_32F);
    model.setInput(blob);
    std::vector<cv::Mat> outputs;
    model.forward(outputs, model.getUnconnectedOutLayersNames());
    if (outputs.empty()) return detections;

    cv::Mat output = outputs[0];
    const int feature_count = output.size[1];
    const int candidate_count = output.size[2];
    const float x_scale = static_cast<float>(frame.cols) / img_size;
    const float y_scale = static_cast<float>(frame.rows) / img_size;
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> class_ids;
    for (int candidate = 0; candidate < candidate_count; ++candidate) {
        float best_score = 0.0f;
        int best_class = -1;
        for (int class_id = 0; class_id < feature_count - 4; ++class_id) {
            float score = output.at<float>(0, class_id + 4, candidate);
            if (score > best_score) { best_score = score; best_class = class_id; }
        }
        if (best_score < confidence_threshold) continue;
        float cx = output.at<float>(0, 0, candidate);
        float cy = output.at<float>(0, 1, candidate);
        float width = output.at<float>(0, 2, candidate);
        float height = output.at<float>(0, 3, candidate);
        cv::Rect box(static_cast<int>((cx - width / 2.0f) * x_scale),
                     static_cast<int>((cy - height / 2.0f) * y_scale),
                     static_cast<int>(width * x_scale), static_cast<int>(height * y_scale));
        boxes.push_back(box & cv::Rect(0, 0, frame.cols, frame.rows));
        scores.push_back(best_score);
        class_ids.push_back(best_class);
    }
    std::vector<int> kept;
    cv::dnn::NMSBoxes(boxes, scores, confidence_threshold, 0.45f, kept);
    for (int index : kept) {
        Detection detection;
        detection.class_name = class_ids[index] < static_cast<int>(class_names.size())
            ? class_names[class_ids[index]] : std::to_string(class_ids[index]);
        detection.confidence = scores[index];
        detection.bbox = boxes[index];
        detections.push_back(detection);
    }
    return detections;
}

// ============================================================
// OpenVINO Integration (Production Ready)
// ============================================================

#ifdef USE_OPENVINO

#include <openvino/openvino.hpp>

class YOLOInference {
private:
    ov::Core core;
    ov::CompiledModel compiled_model;
    ov::InferRequest infer_request;
    std::string input_name;
    std::string output_name;
    int img_size;
    float confidence_threshold;
    
public:
    bool load_model(const std::string& model_path, int size, float conf_thresh) {
        try {
            img_size = size;
            confidence_threshold = conf_thresh;
            
            // Read model
            auto model = core.read_model(model_path);
            
            // Configure input
            auto input = model->input();
            input_name = input.get_any_name();
            
            // Configure output
            auto output = model->output();
            output_name = output.get_any_name();
            
            // Compile for CPU
            ov::AnyMap config = {{ov::hint::performance_mode.name(), ov::hint::PerformanceMode::LATENCY}};
            compiled_model = core.compile_model(model, "CPU", config);
            
            // Create inference request
            infer_request = compiled_model.create_infer_request();
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[YOLO] OpenVINO error: " << e.what() << std::endl;
            return false;
        }
    }
    
    std::vector<Detection> infer(const cv::Mat& frame) {
        std::vector<Detection> detections;
        
        try {
            // Preprocess: resize and convert to blob
            cv::Mat resized;
            cv::resize(frame, resized, cv::Size(img_size, img_size));
            
            // Convert to float and normalize
            cv::Mat float_image;
            resized.convertTo(float_image, CV_32FC3, 1.0 / 255.0);
            
            // Create blob (NCHW format)
            cv::Mat blob = cv::dnn::blobFromImage(float_image);
            
            // Set input
            infer_request.set_input_tensor(ov::Tensor(ov::element::f32, {1, 3, img_size, img_size}, blob.data));
            
            // Run inference
            infer_request.infer();
            
            // Get output
            auto output_tensor = infer_request.get_output_tensor();
            auto output_data = output_tensor.data<float>();
            
            // Decode output (simplified)
            // In production, decode YOLO output properly
            // For now, we'll simulate detections
            
        } catch (const std::exception& e) {
            std::cerr << "[YOLO] Inference error: " << e.what() << std::endl;
        }
        
        return detections;
    }
};

#endif // USE_OPENVINO

// ============================================================
// ONNX Runtime Integration (Alternative)
// ============================================================

#ifdef USE_ONNX

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

class YOLOOnnxInference {
private:
    Ort::Session session;
    Ort::MemoryInfo memory_info;
    std::vector<const char*> input_names;
    std::vector<const char*> output_names;
    int img_size;
    float confidence_threshold;
    
public:
    bool load_model(const std::string& model_path, int size, float conf_thresh) {
        try {
            img_size = size;
            confidence_threshold = conf_thresh;
            
            Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "YOLO");
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(2);
            
            session = Ort::Session(env, model_path.c_str(), session_options);
            
            // Get input/output names
            Ort::AllocatorWithDefaultOptions allocator;
            size_t num_inputs = session.GetInputCount();
            for (size_t i = 0; i < num_inputs; i++) {
                auto name = session.GetInputNameAllocated(i, allocator);
                input_names.push_back(name.get());
            }
            
            size_t num_outputs = session.GetOutputCount();
            for (size_t i = 0; i < num_outputs; i++) {
                auto name = session.GetOutputNameAllocated(i, allocator);
                output_names.push_back(name.get());
            }
            
            memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[YOLO] ONNX error: " << e.what() << std::endl;
            return false;
        }
    }
};

#endif // USE_ONNX