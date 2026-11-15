"""
Security Monitor for Robot V2

Main monitoring loop with:
- Camera capture (10 FPS)
- YOLO detection (Person, Weapon, Fire)
- Alert rules (time/location based)
- Telegram alerts
- State sharing with C++ main
"""

import os
import sys
import cv2
import time
import json
import logging
import tempfile
import threading
import numpy as np
from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, Any, List
from python.constants import STATUS_FILE, ALERT_FILE, DETECTION_FILE

# Add parent directory to path
PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

# ============================================================
# Imports
# ============================================================

from python.camera_handler import CameraHandler, create_camera_or_exit
from python.alert_rules import should_alert, get_time_period
from python.send_telegram_photo import (
    send_telegram_photo,
    send_telegram_message,
    send_telegram_alert
)

# Try to import YOLO OpenVINO
try:
    from src.ai import YOLOOpenVINO
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False
    print("⚠️ YOLO OpenVINO not available. Install with:")
    print("   pip install openvino ultralytics")

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# ============================================================
# Configuration
# ============================================================

# Camera settings
CAMERA_WIDTH = int(os.getenv("CAMERA_WIDTH", "640"))
CAMERA_HEIGHT = int(os.getenv("CAMERA_HEIGHT", "480"))
CAMERA_FPS = int(os.getenv("CAMERA_FPS", "10"))

# YOLO settings
YOLO_MODEL_XML = os.getenv(
    "YOLO_MODEL_XML", str(PROJECT_ROOT / "models" / "best_openvino_model" / "best.xml")
)
YOLO_MODEL_BIN = os.getenv(
    "YOLO_MODEL_BIN", str(PROJECT_ROOT / "models" / "best_openvino_model" / "best.bin")
)
YOLO_CONFIDENCE = float(os.getenv("YOLO_CONFIDENCE", "0.5"))
YOLO_IMG_SIZE = int(os.getenv("YOLO_IMG_SIZE", "640"))

# Alert settings
CONSECUTIVE_FRAMES_REQUIRED = int(os.getenv("CONSECUTIVE_FRAMES_REQUIRED", "3"))
ALERT_COOLDOWN = int(os.getenv("ALERT_COOLDOWN", "5"))
ALERT_FOLDER = os.getenv("ALERT_FOLDER", "alerts")

# Detection classes
TARGET_CLASSES = ["person", "weapon", "fire"]

# File paths for C++ communication


# ============================================================
# Shared State (for C++ integration)
# ============================================================

class DetectionState:
    """Shared state between Python monitor and C++ main."""
    
    def __init__(self):
        self.person_count = 0
        self.weapon_count = 0
        self.fire_count = 0
        self.person_detected = False
        self.weapon_detected = False
        self.fire_detected = False
        self.current_location = "Unknown"
        self.last_detection_time = None
        self.consecutive_frames = 0
        self.last_alert_time = 0
        self.paused = False
        
        # Create alert folder
        os.makedirs(ALERT_FOLDER, exist_ok=True)
    
    def update(self, detections: List[Dict], location: str = "Unknown"):
        """Update state from detections."""
        self.person_count = 0
        self.weapon_count = 0
        self.fire_count = 0
        self.person_detected = False
        self.weapon_detected = False
        self.fire_detected = False
        self.current_location = location
        
        for det in detections:
            cls = det.get('class', '').lower()
            if cls == "person":
                self.person_count += 1
                self.person_detected = True
            elif cls == "weapon":
                self.weapon_count += 1
                self.weapon_detected = True
            elif cls == "fire":
                self.fire_count += 1
                self.fire_detected = True
        
        # Update consecutive frames
        if self.person_detected or self.weapon_detected or self.fire_detected:
            self.consecutive_frames += 1
        else:
            self.consecutive_frames = 0
        
        if self.consecutive_frames > 0:
            self.last_detection_time = datetime.now()
        
        # Write to file for C++ main
        self.write_to_file()
    
    def write_to_file(self):
        """Write detection state to file for C++ main."""
        try:
            data = {
                "person_count": self.person_count,
                "weapon_count": self.weapon_count,
                "fire_count": self.fire_count,
                "person_detected": self.person_detected,
                "weapon_detected": self.weapon_detected,
                "fire_detected": self.fire_detected,
                "location": self.current_location,
                "timestamp": time.time()
            }
            with open(DETECTION_FILE, 'w') as f:
                json.dump(data, f)
        except Exception as e:
            logger.error(f"Failed to write detection state: {e}")
    
    def should_alert(self, location: str = None) -> tuple:
        """Check if alert should be triggered."""
        if not location:
            location = self.current_location
        
        # Check if enough consecutive frames
        if self.consecutive_frames < CONSECUTIVE_FRAMES_REQUIRED:
            return False, "Not enough consecutive frames"
        
        # Check cooldown
        if time.time() - self.last_alert_time < ALERT_COOLDOWN:
            return False, "Cooldown active"
        
        # Use alert rules
        alert, reason = should_alert(
            person_count=self.person_count,
            vehicle_count=0,
            weapon_count=self.weapon_count,
            fire_count=self.fire_count,
            location=location,
            dt=datetime.now()
        )
        
        if alert:
            self.last_alert_time = time.time()
            # Reset consecutive frames after alert
            self.consecutive_frames = 0
        
        return alert, reason

# Global state
state = DetectionState()

# ============================================================
# Monitor Class
# ============================================================

class SecurityMonitor:
    """
    Main security monitor with camera, detection, and alerts.
    """
    
    def __init__(self):
        self.running = False
        self.camera = None
        self.detector = None
        self.thread = None
        
        # Initialize components
        self._init_camera()
        self._init_detector()
        
        logger.info("✅ Security Monitor initialized")
    
    def _init_camera(self):
        """Initialize camera."""
        try:
            self.camera = CameraHandler(
                camera_type="auto",
                width=CAMERA_WIDTH,
                height=CAMERA_HEIGHT,
                fps=CAMERA_FPS
            )
            logger.info(f"📷 Camera ready: {self.camera.get_camera_info()}")
            self._send_camera_startup_frame()
        except Exception as e:
            logger.error(f"❌ Camera init failed: {e}")
            raise

    def _send_camera_startup_frame(self):
        """Send one frame from the active camera when the monitor starts."""
        success, frame = self.camera.read()
        if not success or frame is None:
            logger.error("❌ Could not capture camera startup frame")
            return

        photo_path = None
        try:
            with tempfile.NamedTemporaryFile(suffix=".jpg", delete=False) as photo:
                photo_path = photo.name
            if not cv2.imwrite(photo_path, frame):
                logger.error("❌ Could not save camera startup frame")
                return

            sent = send_telegram_photo(
                photo_path,
                caption="📸 Security robot camera started"
            )
            if sent:
                logger.info("📨 Camera startup frame sent to Telegram")
            else:
                logger.error("❌ Failed to send camera startup frame to Telegram")
        except Exception as e:
            logger.error(f"❌ Camera startup photo error: {e}")
        finally:
            if photo_path and os.path.exists(photo_path):
                try:
                    os.remove(photo_path)
                except OSError as e:
                    logger.warning(f"⚠️ Could not remove temporary startup photo: {e}")
    
    def _init_detector(self):
        """Initialize YOLO detector."""
        if not YOLO_AVAILABLE:
            logger.warning("⚠️ YOLO not available - object detection is disabled")
            return
        
        try:
            self.detector = YOLOOpenVINO(
                model_xml_path=YOLO_MODEL_XML,
                model_bin_path=YOLO_MODEL_BIN,
                confidence_threshold=YOLO_CONFIDENCE,
                nms_threshold=0.45,
                input_size=YOLO_IMG_SIZE
            )
            logger.info("🧠 YOLO detector ready")
        except Exception as e:
            logger.warning(f"⚠️ YOLO init failed: {e}")
            self.detector = None
    
    def detect(self, frame: np.ndarray) -> List[Dict]:
        """
        Detect objects in frame.
        Returns list of detections.
        """
        if self.detector is None:
            return []
        
        try:
            return self.detector.detect(frame)
        except Exception as e:
            logger.error(f"❌ Detection error: {e}")
            return []
    
    def draw_detections(self, frame: np.ndarray, detections: List[Dict]) -> np.ndarray:
        """Draw detection boxes on frame."""
        colors = {
            'person': (0, 255, 0),
            'weapon': (0, 0, 255),
            'fire': (0, 165, 255)
        }
        
        for det in detections:
            cls = det.get('class', '').lower()
            conf = det.get('confidence', 0)
            bbox = det.get('bbox')
            
            if not bbox or len(bbox) < 4:
                continue
            
            x1, y1, x2, y2 = bbox[:4]
            color = colors.get(cls, (255, 255, 255))
            
            # Draw rectangle
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            
            # Draw label
            label = f"{cls} {conf:.2f}"
            cv2.putText(frame, label, (x1, y1 - 10),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
        
        return frame
    
    def process_frame(self, frame: np.ndarray, location: str = "Unknown") -> bool:
        """
        Process a single frame: detect and handle alerts.
        Returns True if alert was triggered.
        """
        if frame is None:
            return False
        
        # Run detection
        detections = self.detect(frame)
        
        # Update state
        state.update(detections, location)
        
        # Draw detections
        frame = self.draw_detections(frame, detections)
        
        # Check for alerts
        alert, reason = state.should_alert(location)
        
        if alert:
            self.handle_alert(reason, frame, detections)
            return True
        
        return False
    
    def handle_alert(self, reason: str, frame: np.ndarray, detections: List[Dict]):
        """Handle alert: save photo, send Telegram."""
        # Build alert type
        alert_type = "ALERT"
        if state.person_detected:
            alert_type = "PERSON_DETECTED"
        elif state.weapon_detected:
            alert_type = "WEAPON_DETECTED"
        elif state.fire_detected:
            alert_type = "FIRE_DETECTED"
        
        # Save photo
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        photo_path = f"{ALERT_FOLDER}/alert_{timestamp}.jpg"
        cv2.imwrite(photo_path, frame)
        logger.info(f"📸 Alert photo saved: {photo_path}")
        
        # Build message
        message = f"{reason}\nLocation: {state.current_location}"
        
        # Send alert
        try:
            send_telegram_alert(
                alert_type=alert_type,
                message=message,
                photo_path=photo_path,
                extra_data={
                    "Persons": state.person_count,
                    "Weapons": state.weapon_count,
                    "Fires": state.fire_count
                }
            )
            logger.info(f"📨 Alert sent: {alert_type}")
        except Exception as e:
            logger.error(f"❌ Alert send failed: {e}")
        
        # Write to alert file for C++ main
        try:
            with open(ALERT_FILE, 'w') as f:
                f.write(f"{alert_type}\n")
                f.write(f"Reason: {reason}\n")
                f.write(f"Photo: {photo_path}\n")
                f.write(f"Time: {datetime.now().isoformat()}\n")
        except Exception as e:
            logger.error(f"Failed to write alert file: {e}")
    
    def run(self, location: str = "Unknown"):
        """
        Main monitoring loop.
        """
        self.running = True
        logger.info("🚀 Starting monitoring loop...")
        
        frame_count = 0
        start_time = time.time()
        
        while self.running:
            try:
                # Capture frame
                ret, frame = self.camera.read()
                if not ret:
                    logger.warning("⚠️ Failed to capture frame")
                    time.sleep(0.1)
                    continue
                
                # Process frame
                alert = self.process_frame(frame, location)
                
                # Status every 10 frames
                frame_count += 1
                if frame_count % 10 == 0:
                    elapsed = time.time() - start_time
                    fps = frame_count / elapsed if elapsed > 0 else 0
                    logger.info(f"📊 FPS: {fps:.1f} | Persons: {state.person_count}")
                
                # Sleep to maintain FPS
                time.sleep(1 / CAMERA_FPS)
                
            except KeyboardInterrupt:
                logger.info("👋 Stop requested")
                break
            except Exception as e:
                logger.error(f"❌ Monitor error: {e}")
                time.sleep(1)
        
        self.stop()
    
    def stop(self):
        """Stop monitoring."""
        self.running = False
        if self.camera:
            self.camera.release()
        logger.info("✅ Monitor stopped")
    
    def run_in_background(self, location: str = "Unknown"):
        """Run monitor in background thread."""
        self.thread = threading.Thread(
            target=self.run,
            args=(location,),
            daemon=True
        )
        self.thread.start()
        return self.thread

# ============================================================
# Helper Functions
# ============================================================

def monitor_camera(location: str = "Unknown"):
    """
    Wrapper function for main.py integration.
    Starts the monitor and runs it.
    """
    monitor = SecurityMonitor()
    monitor.run(location)
    return True

def run_monitor_in_background(location: str = "Unknown"):
    """
    Run monitor in background thread.
    """
    monitor = SecurityMonitor()
    return monitor.run_in_background(location)

# ============================================================
# Alert Monitor (C++ integration)
# ============================================================

def monitor_alerts():
    """Monitor alert file for triggers from C++ main."""
    logger.info("📡 Alert monitor started")
    
    while True:
        try:
            if os.path.exists(ALERT_FILE):
                with open(ALERT_FILE, 'r') as f:
                    content = f.read().strip()
                
                # Clear file
                os.remove(ALERT_FILE)
                
                if content:
                    lines = content.split('\n')
                    alert_type = lines[0] if lines else "ALERT"
                    
                    # Send alert
                    message = "Alert from C++ main\n" + "\n".join(lines[1:])
                    send_telegram_alert(
                        alert_type=alert_type,
                        message=message
                    )
                    logger.info(f"📨 Alert sent from C++: {alert_type}")
        
        except Exception as e:
            logger.error(f"Alert monitor error: {e}")
        
        time.sleep(0.5)

def run_alert_monitor_in_background():
    """Run alert monitor in background thread."""
    thread = threading.Thread(target=monitor_alerts, daemon=True)
    thread.start()
    return thread

# ============================================================
# Main Entry Point
# ============================================================

if __name__ == "__main__":
    print("🔱 TRISHIVRA Security Monitor")
    print("=" * 40)
    print(f"📷 Camera: {CAMERA_WIDTH}x{CAMERA_HEIGHT} @ {CAMERA_FPS} FPS")
    print(f"🧠 YOLO: {'Loaded' if YOLO_AVAILABLE else 'Mock mode'}")
    print("=" * 40)
    print("Press Ctrl+C to stop\n")
    
    try:
        monitor_camera()
    except KeyboardInterrupt:
        print("\n👋 Shutting down...")