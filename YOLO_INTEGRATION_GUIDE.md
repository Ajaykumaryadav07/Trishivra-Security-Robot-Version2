# 🎯 Real YOLO Integration Guide - Security Robot V2

**Goal:** Replace simulated detections with real person/weapon/fire detection

---

## 📋 STEP 1: Choose YOLO Model & Framework

### Option A: **OpenVINO (Recommended for Raspberry Pi)**
- ✅ Optimized for CPU inference
- ✅ Works on Pi without GPU
- ✅ ~50-100ms per frame (6 FPS feasible)
- ✅ Model size: ~100-200MB

### Option B: **ONNX Runtime**
- ✅ Cross-platform (Pi, desktop, cloud)
- ✅ Good CPU performance
- ✅ ~80-150ms per frame

### Option C: **TensorFlow Lite (fastest)**
- ✅ Smallest model size (~50MB)
- ✅ Fastest inference (~30-50ms)
- ⚠️ Requires TFLite training pipeline
- ⚠️ Less mature for custom classes

**👉 RECOMMENDED: OpenVINO (OpenCV native integration)**

---

## 🏋️ STEP 2: Get/Train YOLO Model

### Option A: Use Pre-trained YOLO v8 (EASIEST)

```bash
# Install YOLOv8
pip install ultralytics

# Download pre-trained YOLOv8 weights (COCO dataset)
python3 << 'EOF'
from ultralytics import YOLO

# Load YOLOv8n (nano - fastest, smallest)
model = YOLO('yolov8n.pt')

# This downloads automatically to ~/.cache/hub/
# You need: yolov8n.pt (~6MB)
EOF
```

**BUT:** Pre-trained COCO won't detect "weapon" or "fire"

### Option B: Train Custom YOLO v8 (RECOMMENDED)

```bash
# 1. Prepare dataset with images labeled for:
#    - person
#    - weapon
#    - fire
# 
# Dataset structure:
# custom_dataset/
# ├── images/
# │   ├── train/
# │   └── val/
# └── labels/
#     ├── train/
#     └── val/

# 2. Create dataset.yaml
cat > dataset.yaml << 'EOF'
path: /path/to/custom_dataset
train: images/train
val: images/val

nc: 3  # number of classes
names: ['person', 'weapon', 'fire']
EOF

# 3. Train custom YOLO v8n
python3 << 'PYEOF'
from ultralytics import YOLO

model = YOLO('yolov8n.pt')  # Start with nano model
results = model.train(
    data='dataset.yaml',
    epochs=100,
    imgsz=320,  # 320x320 for speed
    device=0,   # GPU device (0) or 'cpu'
    patience=20,
    save=True
)

# Export to OpenVINO
model = YOLO('runs/detect/train/weights/best.pt')
model.export(format='openvino', imgsz=320)
PYEOF
```

---

## 📦 STEP 3: Convert Model to OpenVINO Format

```bash
# After YOLOv8 training, export automatically to OpenVINO:
# Creates: runs/detect/train/weights/best_openvino_model/

# Files generated:
# ├── best.onnx
# ├── best_openvino_model/
# │   ├── best.xml         ← OpenVINO IR (model definition)
# │   ├── best.bin         ← OpenVINO weights
# │   └── best_openvino_model.yaml
```

**Copy these to your robot:**
```bash
scp -r best_openvino_model/ pi@raspberrypi:/home/pi/security_robot_v2/models/
```

---

## 💻 STEP 4: Install Dependencies on Raspberry Pi

```bash
# On Raspberry Pi 4/5 running Raspberry Pi OS

# Install OpenVINO Runtime (lightweight)
pip3 install openvino

# Install OpenCV with OpenVINO support
sudo apt-get install -y libopenvino-dev opencv-data

# Verify installation
python3 -c "import openvino; print(openvino.__version__)"
```

---

## 🔧 STEP 5: Modify CMakeLists.txt (C++ Build)

```cmake
# Add to your security_robot_v2/CMakeLists.txt

find_package(OpenVINO REQUIRED)

target_link_libraries(security_robot 
    ${OpenCV_LIBS}
    ${OpenVINO_LIBRARIES}  # Add this
)

# Include OpenVINO headers
target_include_directories(security_robot PRIVATE ${OpenVINO_INCLUDE_DIRS})
```

---

## 🚀 STEP 6: Update C++ Code (yolo_detector.cpp)

### Replace the simulate detector with real OpenVINO inference:

```cpp
#include "yolo_detector.h"
#include <openvino/openvino.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

class YOLODetector::Impl {
public:
    ov::Core core;
    ov::CompiledModel compiled_model;
    ov::InferRequest infer_request;
    std::string input_name;
    std::string output_name;
    int img_size;
    float confidence_threshold;
    
    bool load_model(const std::string& model_dir, int size, float conf_thresh) {
        try {
            img_size = size;
            confidence_threshold = conf_thresh;
            
            // Load OpenVINO model from directory
            std::string model_path = model_dir + "/best.xml";
            std::cout << "[YOLO] Loading model from: " << model_path << std::endl;
            
            // Read model
            auto model = core.read_model(model_path);
            
            // Get input/output names
            auto input = model->input();
            input_name = input.get_any_name();
            
            auto output = model->output();
            output_name = output.get_any_name();
            
            std::cout << "[YOLO] Input: " << input_name << std::endl;
            std::cout << "[YOLO] Output: " << output_name << std::endl;
            
            // Compile for CPU (Pi)
            compiled_model = core.compile_model(model, "CPU");
            infer_request = compiled_model.create_infer_request();
            
            std::cout << "[YOLO] Model compiled for CPU" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[YOLO] Load error: " << e.what() << std::endl;
            return false;
        }
    }
    
    std::vector<Detection> detect(const cv::Mat& frame) {
        std::vector<Detection> detections;
        
        if (frame.empty()) return detections;
        
        try {
            // 1. PREPROCESS: Resize and normalize
            cv::Mat resized;
            cv::resize(frame, resized, cv::Size(img_size, img_size));
            
            // Convert BGR to RGB
            cv::Mat rgb;
            cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
            
            // Normalize to [0, 1]
            rgb.convertTo(rgb, CV_32F, 1.0/255.0);
            
            // Create blob (1, 3, 320, 320)
            ov::Tensor input_tensor(ov::element::f32, 
                                   {1, 3, img_size, img_size},
                                   rgb.data);
            
            // 2. RUN INFERENCE
            infer_request.set_input_tensor(input_tensor);
            infer_request.infer();
            
            // 3. POSTPROCESS: Get output
            auto output_tensor = infer_request.get_output_tensor();
            float* output_data = output_tensor.data<float>();
            
            // YOLO output shape: (1, 25200, 7)
            // 25200 = (320/32)^2 * 25 predictions
            // 7 = [x, y, w, h, confidence, person_prob, weapon_prob, fire_prob] simplified
            
            const int num_detections = 25200;
            const int num_classes = 3;  // person, weapon, fire
            const int detection_size = 5 + num_classes;
            
            float scale_x = (float)frame.cols / img_size;
            float scale_y = (float)frame.rows / img_size;
            
            // Process each detection
            for (int i = 0; i < num_detections; i++) {
                float* det = output_data + i * detection_size;
                
                float objectness = det[4];  // Confidence
                if (objectness < confidence_threshold) continue;
                
                // Get class probabilities
                float person_score = det[5];
                float weapon_score = det[6];
                float fire_score = det[7];
                
                // Find max class
                int class_id = 0;
                float class_conf = person_score;
                
                if (weapon_score > class_conf) {
                    class_id = 1;
                    class_conf = weapon_score;
                }
                if (fire_score > class_conf) {
                    class_id = 2;
                    class_conf = fire_score;
                }
                
                if (class_conf < confidence_threshold) continue;
                
                // Decode bounding box
                float cx = det[0] * scale_x;
                float cy = det[1] * scale_y;
                float w = det[2] * scale_x;
                float h = det[3] * scale_y;
                
                int x = (int)(cx - w/2);
                int y = (int)(cy - h/2);
                int width = (int)w;
                int height = (int)h;
                
                // Clamp to frame bounds
                x = std::max(0, x);
                y = std::max(0, y);
                width = std::min(width, frame.cols - x);
                height = std::min(height, frame.rows - y);
                
                // Create detection
                Detection detection;
                detection.bbox = cv::Rect(x, y, width, height);
                detection.confidence = class_conf * objectness;
                
                // Map class ID to name
                if (class_id == 0) detection.class_name = "person";
                else if (class_id == 1) detection.class_name = "weapon";
                else if (class_id == 2) detection.class_name = "fire";
                
                detections.push_back(detection);
            }
            
            // Apply Non-Maximum Suppression (NMS)
            apply_nms(detections, 0.4f);  // IoU threshold 0.4
            
        } catch (const std::exception& e) {
            std::cerr << "[YOLO] Detect error: " << e.what() << std::endl;
        }
        
        return detections;
    }
    
private:
    void apply_nms(std::vector<Detection>& detections, float nms_threshold) {
        if (detections.empty()) return;
        
        // Sort by confidence
        std::sort(detections.begin(), detections.end(),
                 [](const Detection& a, const Detection& b) {
                     return a.confidence > b.confidence;
                 });
        
        std::vector<Detection> result;
        result.push_back(detections[0]);
        
        for (size_t i = 1; i < detections.size(); i++) {
            bool keep = true;
            for (const auto& kept : result) {
                float iou = calculate_iou(detections[i].bbox, kept.bbox);
                if (iou > nms_threshold && detections[i].class_name == kept.class_name) {
                    keep = false;
                    break;
                }
            }
            if (keep) result.push_back(detections[i]);
        }
        
        detections = result;
    }
    
    float calculate_iou(const cv::Rect& a, const cv::Rect& b) {
        int inter_x1 = std::max(a.x, b.x);
        int inter_y1 = std::max(a.y, b.y);
        int inter_x2 = std::min(a.x + a.width, b.x + b.width);
        int inter_y2 = std::min(a.y + a.height, b.y + b.height);
        
        if (inter_x2 < inter_x1 || inter_y2 < inter_y1) return 0.0f;
        
        float inter_area = (inter_x2 - inter_x1) * (inter_y2 - inter_y1);
        float union_area = a.width * a.height + b.width * b.height - inter_area;
        
        return inter_area / union_area;
    }
};

// Update YOLODetector class
bool YOLODetector::init(const std::string& model_path, float threshold) {
    try {
        impl = std::make_unique<Impl>();
        bool success = impl->load_model(model_path, YOLO_IMG_SIZE, threshold);
        
        if (success) {
            initialized = true;
            confidence_threshold = threshold;
            std::cout << "[YOLO] ✅ Real model initialized!" << std::endl;
        } else {
            std::cerr << "[YOLO] ❌ Failed to load model" << std::endl;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLO] Init error: " << e.what() << std::endl;
        return false;
    }
}

std::vector<Detection> YOLODetector::detect(const cv::Mat& frame) {
    if (!initialized || !impl) {
        return std::vector<Detection>();
    }
    return impl->detect(frame);
}
```

---

## 📝 Update yolo_detector.h

```cpp
#ifndef YOLO_DETECTOR_H
#define YOLO_DETECTOR_H

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include "robot_modules.h"

class YOLODetector {
public:
    YOLODetector();
    ~YOLODetector();

    bool init(const std::string& model_path, float threshold = 0.5f);
    std::vector<Detection> detect(const cv::Mat& frame);
    bool is_initialized() const { return initialized; }

private:
    class Impl;
    std::unique_ptr<Impl> impl;
    float confidence_threshold;
    bool initialized;
};

#endif
```

---

## 🔧 Update main.cpp to Load Real Model

```cpp
// In main.cpp, replace:
YOLODetector yolo;
if (!yolo.init(YOLO_MODEL_PATH, YOLO_CONFIDENCE_THRESHOLD)) {
    std::cerr << "[MAIN] ⚠️ YOLO init failed!" << std::endl;
    std::cout << "[MAIN] Running without YOLO detection" << std::endl;
}

// With:
YOLODetector yolo;
std::string model_dir = "/home/pi/security_robot_v2/models/best_openvino_model";
if (!yolo.init(model_dir, 0.5f)) {
    std::cerr << "[MAIN] ❌ YOLO init failed!" << std::endl;
    return 1;  // Critical - stop if YOLO unavailable
}
std::cout << "[MAIN] ✅ YOLO detector ready (person/weapon/fire)" << std::endl;
```

---

## 📊 Performance Expectations

| Aspect | Value |
|--------|-------|
| Model Size | ~100-200MB |
| Inference Time | 60-100ms per frame |
| FPS Impact | 6 FPS maintained (each frame takes 100ms max) |
| Accuracy | ~85-90% (depends on training data) |
| False Positive Rate | ~5-10% (tuned via NMS threshold) |
| RAM Usage | ~200-300MB |

---

## 🧪 Testing Without Training (Use Existing Model)

If you don't want to train, you can use **YOLOv8 + custom fine-tuning**:

```bash
# Download pre-trained YOLOv8n-seg (segmentation)
python3 << 'EOF'
from ultralytics import YOLO

# This has generic person detection
model = YOLO('yolov8n-seg.pt')

# Export to OpenVINO
model.export(format='openvino', imgsz=320)
EOF
```

**⚠️ Limitations:**
- Won't detect "weapon" or "fire" (not in COCO)
- Only detects "person" class
- Need to train custom model for full functionality

---

## 📚 Dataset Preparation Tips

### Collect Training Data:
1. **Person images:** 500-1000 images (various poses, lighting)
2. **Weapon images:** 300-500 images (guns, knives, etc.)
3. **Fire images:** 300-500 images (flames, smoke, etc.)

### Tools:
- **Roboflow:** Auto-splits into train/val, formats for YOLO
- **Labelimg:** Manual annotation tool
- **Albumentations:** Data augmentation for more training samples

```bash
# Augment dataset to 5000+ images
pip install albumentations
python3 augment_dataset.py --input original_dataset --output augmented_dataset --multiplier 5
```

---

## ✅ Final Checklist

- [ ] Downloaded/trained YOLO v8 model
- [ ] Exported to OpenVINO format (best.xml + best.bin)
- [ ] Copied model to Pi: `/home/pi/security_robot_v2/models/best_openvino_model/`
- [ ] Updated CMakeLists.txt with OpenVINO
- [ ] Installed OpenVINO runtime on Pi
- [ ] Updated yolo_detector.cpp with real inference code
- [ ] Compiled C++ project
- [ ] Tested on sample video/image
- [ ] Verified person/weapon/fire detection works
- [ ] Tuned confidence threshold for your environment

---

**Next:** Once model is ready, recompile and test on real robot! 🚀

