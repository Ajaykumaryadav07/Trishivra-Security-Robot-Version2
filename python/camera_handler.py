"""
Camera Handler for Security Robot V2

Supports:
- Raspberry Pi Camera Module 2 (NoIR)
- USB Webcams
- Auto-detection with fallback
- 6 FPS (configurable)
- OpenCV compatibility
"""

try:
    import cv2
    CV2_AVAILABLE = True
except Exception:
    cv2 = None
    CV2_AVAILABLE = False
import os
import time
import logging
from typing import Tuple, Optional
from threading import Lock

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Try to import Pi Camera library
try:
    from picamera2 import Picamera2
    PI_CAMERA_AVAILABLE = True
except ImportError:
    PI_CAMERA_AVAILABLE = False
    logger.warning("⚠️ picamera2 not available - using USB camera only")


class CameraHandler:
    """
    Unified camera interface for Pi Camera Module and USB webcams.
    Auto-detects available camera and provides unified interface.
    Optimized for 6 FPS on Raspberry Pi.
    """
    
    def __init__(self, 
                 camera_type: str = "auto",
                 width: int = 640,
                 height: int = 480,
                 fps: int = 6):
        """
        Initialize camera handler.
        
        Args:
            camera_type: "pi_camera", "usb", or "auto" (auto-detect)
            width: Frame width (default: 640)
            height: Frame height (default: 480)
            fps: Frames per second (default: 6)
        """
        self.width = width
        self.height = height
        self.fps = fps
        self.camera_type = None
        self.pi_camera = None
        self.usb_camera = None
        self.is_initialized = False
        self.lock = Lock()
        
        self._init_camera(camera_type)
    
    def _init_camera(self, camera_type: str):
        """Initialize appropriate camera type."""
        
        if camera_type == "auto":
            # Try Pi Camera first
            if PI_CAMERA_AVAILABLE:
                try:
                    logger.info("🎥 Attempting Pi Camera initialization...")
                    self.pi_camera = Picamera2()
                    self.camera_type = "pi_camera"
                    self._setup_pi_camera()
                    self.is_initialized = True
                    logger.info("✅ Pi Camera Module connected")
                    return
                except Exception as e:
                    logger.warning(f"⚠️ Pi Camera failed: {e}")
                    self.pi_camera = None

            # Fallback to USB camera
            if not CV2_AVAILABLE:
                logger.warning("⚠️ OpenCV (cv2) not available — skipping USB camera initialization")
                raise RuntimeError("❌ No camera found (Pi Camera not available and OpenCV missing)")

            logger.info("🎥 Attempting USB camera initialization...")
            usb_indexes = [int(os.getenv("USB_CAMERA_INDEX", "0"))]
            if usb_indexes[0] != 0:
                usb_indexes.append(0)
            else:
                usb_indexes.extend([1, 2, 3])

            for idx in usb_indexes:
                try:
                    self.usb_camera = cv2.VideoCapture(idx)
                except Exception as e:
                    logger.warning(f"⚠️ Failed to open USB camera index {idx}: {e}")
                    self.usb_camera = None
                    continue

                if self.usb_camera and self.usb_camera.isOpened():
                    self.camera_type = "usb"
                    self._setup_usb_camera()
                    self.is_initialized = True
                    logger.info(f"✅ USB Webcam connected at index {idx}")
                    return
                try:
                    if self.usb_camera:
                        self.usb_camera.release()
                except Exception:
                    pass

            raise RuntimeError("❌ No camera found (Pi Camera or USB)")
        
        elif camera_type == "pi_camera":
            if not PI_CAMERA_AVAILABLE:
                raise RuntimeError("❌ picamera2 not installed")
            self.pi_camera = Picamera2()
            self.camera_type = "pi_camera"
            self._setup_pi_camera()
            self.is_initialized = True
            logger.info("✅ Pi Camera Module connected")
        
        elif camera_type == "usb":
            usb_indexes = [int(os.getenv("USB_CAMERA_INDEX", "0"))]
            if usb_indexes[0] != 0:
                usb_indexes.append(0)
            else:
                usb_indexes.extend([1, 2, 3])

            for idx in usb_indexes:
                self.usb_camera = cv2.VideoCapture(idx)
                if self.usb_camera.isOpened():
                    self.camera_type = "usb"
                    self._setup_usb_camera()
                    self.is_initialized = True
                    logger.info(f"✅ USB Webcam connected at index {idx}")
                    return
                self.usb_camera.release()

            raise RuntimeError("❌ USB camera not found")
    
    def _setup_pi_camera(self):
        """Configure Pi Camera Module."""
        if self.pi_camera is None:
            return
        
        config = self.pi_camera.create_preview_configuration(
            main={"format": "RGB888", "size": (self.width, self.height)}
        )
        self.pi_camera.configure(config)
        self.pi_camera.start()
        
        # Allow camera to warm up
        time.sleep(0.5)
    
    def _setup_usb_camera(self):
        """Configure USB camera."""
        if self.usb_camera is None:
            return

        if not CV2_AVAILABLE:
            return

        try:
            self.usb_camera.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
            self.usb_camera.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
            self.usb_camera.set(cv2.CAP_PROP_FPS, self.fps)
        except Exception as e:
            logger.warning(f"⚠️ Could not configure USB camera properties: {e}")

        # Let camera adjust
        time.sleep(0.5)
    
    def read(self) -> Tuple[bool, Optional[object]]:
        """
        Capture a frame from the camera.
        
        Returns:
            Tuple of (success, frame) where frame is numpy array in BGR format
        """
        if not self.is_initialized:
            return False, None
        
        with self.lock:
            try:
                if self.camera_type == "pi_camera" and self.pi_camera:
                    request = self.pi_camera.capture_request()
                    frame = request.make_array("main")
                    request.release()
                    # Convert RGB to BGR for OpenCV compatibility
                    frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                    return True, frame
                
                elif self.camera_type == "usb" and self.usb_camera:
                    if not CV2_AVAILABLE:
                        return False, None

                    try:
                        ret, frame = self.usb_camera.read()
                        if ret and frame is not None:
                            return True, frame
                        return False, None
                    except Exception as e:
                        logger.error(f"❌ USB camera read error: {e}")
                        return False, None
                
                else:
                    return False, None
                    
            except Exception as e:
                logger.error(f"❌ Camera read error: {e}")
                return False, None
    
    def release(self):
        """Release camera resources."""
        try:
            if self.pi_camera:
                self.pi_camera.stop()
                self.pi_camera = None
                logger.info("📷 Pi Camera released")
            
            if self.usb_camera:
                self.usb_camera.release()
                self.usb_camera = None
                logger.info("📷 USB Camera released")
            
            self.is_initialized = False
            
        except Exception as e:
            logger.error(f"❌ Error releasing camera: {e}")
    
    def get_camera_info(self) -> dict:
        """Get camera type and resolution info."""
        return {
            "type": self.camera_type,
            "width": self.width,
            "height": self.height,
            "fps": self.fps,
            "model": "Pi Camera Module" if self.camera_type == "pi_camera" else "USB Webcam",
            "initialized": self.is_initialized
        }
    
    def is_opened(self) -> bool:
        """Check if camera is open and initialized."""
        return self.is_initialized
    
    def set_resolution(self, width: int, height: int):
        """Change camera resolution (re-initializes camera)."""
        if self.is_initialized:
            self.release()
        self.width = width
        self.height = height
        self._init_camera("auto")
    
    def set_fps(self, fps: int):
        """Change camera FPS (re-initializes camera)."""
        if self.is_initialized:
            self.release()
        self.fps = fps
        self._init_camera("auto")
    
    def __del__(self):
        """Ensure camera is released on object deletion."""
        self.release()


# ============================================================
# Helper Functions
# ============================================================

def list_cameras(max_index: int = 5) -> list:
    """
    List all available camera devices.
    
    Args:
        max_index: Maximum camera index to check
    
    Returns:
        List of available camera indices
    """
    available = []
    for i in range(max_index):
        cap = cv2.VideoCapture(i)
        if cap.isOpened():
            available.append(i)
            cap.release()
    return available


def create_camera_or_exit(width: int = 640, height: int = 480, fps: int = 6):
    """
    Create camera handler with error handling.
    Exits if no camera found.
    """
    try:
        camera = CameraHandler(
            camera_type="auto",
            width=width,
            height=height,
            fps=fps
        )
        return camera
    except Exception as e:
        logger.error(f"❌ Failed to initialize camera: {e}")
        exit(1)


# ============================================================
# Test Function
# ============================================================

if __name__ == "__main__":
    print("📸 Testing Camera Handler...")
    
    # Test auto-detection
    camera = CameraHandler(
        camera_type="auto",
        width=640,
        height=480,
        fps=6
    )
    
    print(f"Camera Info: {camera.get_camera_info()}")
    
    # Capture test frames
    print("Capturing test frames (press 'q' to quit)...")
    
    frame_count = 0
    start_time = time.time()
    
    while True:
        ret, frame = camera.read()
        if not ret:
            print("⚠️ Failed to capture frame")
            break
        
        frame_count += 1
        
        # Display FPS
        elapsed = time.time() - start_time
        if elapsed > 0:
            fps = frame_count / elapsed
            cv2.putText(frame, f"FPS: {fps:.1f}", (10, 30),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
        
        cv2.imshow("Camera Test", frame)
        
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    camera.release()
    cv2.destroyAllWindows()
    print("✅ Test complete")