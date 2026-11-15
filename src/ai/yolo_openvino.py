"""
YOLO Detection with OpenVINO for Raspberry Pi
Optimized for low CPU usage on Raspberry Pi 4
"""

import os
import cv2
import numpy as np
import time
import logging
from pathlib import Path

try:
    from openvino.runtime import Core, PartialShape
    OPENVINO_AVAILABLE = True
except ImportError:
    OPENVINO_AVAILABLE = False
    print("⚠️ OpenVINO not installed. Install: pip install openvino")

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class YOLOOpenVINO:
    """
    YOLO Object Detection using OpenVINO INT8 quantization
    Optimized for Raspberry Pi 4 (10 FPS)
    """
    
    def __init__(self,
                 model_xml_path="models/best_openvino_model/best.xml",
                 model_bin_path="models/best_openvino_model/best.bin",
                 confidence_threshold=0.5,
                 nms_threshold=0.45,
                 input_size=640):
        
        self.model_xml_path = model_xml_path
        self.model_bin_path = model_bin_path
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        self.input_size = input_size
        names_path = Path(model_xml_path).with_suffix(".names")
        try:
            self.class_names = [
                line.strip()
                for line in names_path.read_text(encoding="utf-8").splitlines()
                if line.strip()
            ]
        except OSError:
            self.class_names = ["person", "weapon", "vehicle", "fire"]
        self.compiled_model = None
        self.input_layer = None
        self.output_layer = None
        
        self.load_model()
    
    def load_model(self):
        """Load OpenVINO model"""
        if not OPENVINO_AVAILABLE:
            logger.error("❌ OpenVINO not available")
            return False
        
        try:
            # Check if model files exist
            if not os.path.exists(self.model_xml_path):
                logger.warning(f"Model not found: {self.model_xml_path}")
                logger.info("Convert YOLO to OpenVINO with:")
                logger.info("  mo --input_model yolov8n.onnx --input_shape [1,3,320,320] --data_type FP16")
                return False
            
            # Initialize OpenVINO Core
            core = Core()
            
            # Read model
            model = core.read_model(self.model_xml_path)
            
            # Compile for CPU
            self.compiled_model = core.compile_model(model, "CPU")
            
            # Get input/output info
            self.input_layer = self.compiled_model.input(0)
            self.output_layer = self.compiled_model.output(0)
            
            logger.info(f"✅ OpenVINO YOLO loaded: {self.model_xml_path}")
            logger.info(f"   Input: {self.input_layer.shape}")
            logger.info(f"   Output: {self.output_layer.shape}")
            
            return True
            
        except Exception as e:
            logger.error(f"❌ Failed to load model: {e}")
            return False
    
    def preprocess(self, frame):
        """
        Preprocess frame for YOLO inference
        """
        if frame is None:
            return None
        
        # Resize to model input size
        resized = cv2.resize(frame, (self.input_size, self.input_size))
        resized = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
        
        # Convert to float and normalize
        blob = np.array(resized, dtype=np.float32) / 255.0
        
        # Transpose to NCHW (1, 3, 320, 320)
        blob = np.transpose(blob, (2, 0, 1))
        blob = np.expand_dims(blob, axis=0)
        
        return blob
    
    def postprocess(self, output, original_shape):
        """
        Decode YOLO output to bounding boxes
        """
        detections = []
        output = np.asarray(output)
        while output.ndim > 2 and output.shape[0] == 1:
            output = output[0]
        if output.ndim != 2:
            logger.error("Unexpected YOLO output shape: %s", output.shape)
            return []

        feature_count = len(self.class_names) + 4
        if output.shape[0] == feature_count:
            pass
        elif output.shape[1] == feature_count:
            output = output.T
        else:
            logger.error(
                "YOLO output has %s features; expected %s for configured labels",
                min(output.shape),
                feature_count,
            )
            return []

        bbox = output[:4, :]
        scores = output[4:, :]
        
        # Get class with max score
        class_ids = np.argmax(scores, axis=0)        # (8400,)
        max_scores = np.max(scores, axis=0)          # (8400,)
        
        # Filter by confidence
        valid_indices = max_scores > self.confidence_threshold
        bbox = bbox[:, valid_indices]
        class_ids = class_ids[valid_indices]
        max_scores = max_scores[valid_indices]
        
        # YOLOv8 exports center, size coordinates in input-image pixels.
        cx = bbox[0, :]
        cy = bbox[1, :]
        w = bbox[2, :]
        h = bbox[3, :]

        scale_x = original_shape[1] / self.input_size
        scale_y = original_shape[0] / self.input_size
        x1 = (cx - w / 2) * scale_x
        y1 = (cy - h / 2) * scale_y
        x2 = (cx + w / 2) * scale_x
        y2 = (cy + h / 2) * scale_y

        for i in range(len(x1)):
            if class_ids[i] >= len(self.class_names):
                continue

            det = {
                'class': self.class_names[class_ids[i]],
                'confidence': float(max_scores[i]),
                'bbox': [int(x1[i]), int(y1[i]), int(x2[i]), int(y2[i])],
                'class_id': int(class_ids[i]),
            }
            detections.append(det)
        
        return detections
    
    def detect(self, frame):
        """
        Run inference on a frame
        Returns: list of detections
        """
        if self.compiled_model is None or frame is None:
            return []
        
        # Store original dimensions
        orig_h, orig_w = frame.shape[:2]
        
        # Preprocess
        blob = self.preprocess(frame)
        if blob is None:
            return []
        
        try:
            # Run inference
            result = self.compiled_model([blob])[self.output_layer]
            
            # Postprocess
            detections = self.postprocess(result, (orig_h, orig_w))
            
            # Apply NMS
            detections = self.non_max_suppression(detections)
            
            return detections
            
        except Exception as e:
            logger.error(f"Detection error: {e}")
            return []
    
    def non_max_suppression(self, detections):
        """
        Apply Non-Maximum Suppression
        """
        if len(detections) == 0:
            return detections
        
        # Sort by confidence
        detections = sorted(detections, key=lambda x: x['confidence'], reverse=True)
        
        keep = []
        while detections:
            best = detections.pop(0)
            keep.append(best)
            
            # Remove boxes that overlap too much with best
            filtered = []
            for det in detections:
                iou = self.compute_iou(best['bbox'], det['bbox'])
                if iou < self.nms_threshold:
                    filtered.append(det)
            
            detections = filtered
        
        return keep
    
    def compute_iou(self, box1, box2):
        """
        Compute Intersection over Union of two boxes
        """
        x1 = max(box1[0], box2[0])
        y1 = max(box1[1], box2[1])
        x2 = min(box1[2], box2[2])
        y2 = min(box1[3], box2[3])
        
        if x2 < x1 or y2 < y1:
            return 0.0
        
        intersection = (x2 - x1) * (y2 - y1)
        area1 = (box1[2] - box1[0]) * (box1[3] - box1[1])
        area2 = (box2[2] - box2[0]) * (box2[3] - box2[1])
        
        iou = intersection / (area1 + area2 - intersection)
        return iou
    
    def draw_detections(self, frame, detections):
        """
        Draw bounding boxes on frame
        """
        for det in detections:
            x1, y1, x2, y2 = det['bbox']
            cls = det['class']
            conf = det['confidence']
            
            # Color based on class
            if cls == "person":
                color = (0, 255, 0)
            elif cls == "weapon":
                color = (0, 0, 255)
            elif cls == "fire":
                color = (0, 165, 255)
            else:
                color = (255, 0, 0)
            
            # Draw rectangle
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            
            # Draw label
            label = f"{cls} {conf:.2f}"
            cv2.putText(frame, label, (x1, y1 - 10),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
        
        return frame


# ============================================================
# OpenVINO Model Conversion Utility
# ============================================================

def convert_yolo_to_openvino(yolo_model_path="yolov8n.pt",
                              output_dir="models",
                              input_size=320):
    """
    Convert YOLO PyTorch model to OpenVINO
    Requires: ultralytics, openvino-dev
    """
    try:
        from ultralytics import YOLO
        import subprocess
        import os
        
        print(f"📦 Converting YOLO model: {yolo_model_path}")
        
        # Ensure output directory exists
        os.makedirs(output_dir, exist_ok=True)
        
        # Load YOLO model
        model = YOLO(yolo_model_path)
        
        # Export to ONNX
        onnx_path = os.path.join(output_dir, "yolov8n.onnx")
        model.export(format="onnx", imgsz=input_size, opset=12)
        
        print(f"✅ ONNX exported: {onnx_path}")
        
        # Convert ONNX to OpenVINO
        print("🔄 Converting to OpenVINO...")
        xml_path = os.path.join(output_dir, "yolov8n.xml")
        cmd = [
            "mo",
            "--input_model", onnx_path,
            "--input_shape", f"[1,3,{input_size},{input_size}]",
            "--data_type", "FP16",
            "--output_dir", output_dir,
            "--model_name", "yolov8n"
        ]
        subprocess.run(cmd, check=True)
        
        print(f"✅ OpenVINO model saved: {xml_path}")
        return xml_path
        
    except Exception as e:
        print(f"❌ Conversion failed: {e}")
        return None


# ============================================================
# Usage Example
# ============================================================

if __name__ == "__main__":
    from python.camera_handler import CameraHandler

    # Initialize detector
    detector = YOLOOpenVINO(
        model_xml_path="models/yolov8n.xml",
        model_bin_path="models/yolov8n.bin",
        confidence_threshold=0.5,
        nms_threshold=0.45,
        input_size=320
    )
    
    # Test on a frame
    camera = CameraHandler(camera_type="pi_camera", width=640, height=480, fps=10)
    
    print("📸 Starting camera test (Press 'q' to quit)...")
    
    while True:
        ret, frame = camera.read()
        if not ret:
            break
        
        # Detect
        detections = detector.detect(frame)
        
        # Draw
        frame = detector.draw_detections(frame, detections)
        
        # Show FPS
        cv2.putText(frame, f"FPS: 6", (10, 30),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
        
        # Show
        cv2.imshow("YOLO OpenVINO", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
        
        time.sleep(0.1)  # 10 FPS
    
    camera.release()
    cv2.destroyAllWindows()