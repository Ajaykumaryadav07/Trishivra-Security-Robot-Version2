"""
Camera Handler for Security Robot V2

Supports:
- Raspberry Pi Camera Module 2 (NoIR)
- Raspberry Pi camera only
- Pi camera detection
- 10 FPS (configurable)
- OpenCV compatibility
"""

try:
    import cv2
    CV2_AVAILABLE = True
except Exception:
    cv2 = None
    CV2_AVAILABLE = False
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
    logger.warning("⚠️ picamera2 not available - Pi Camera support disabled")


class CameraHandler:
    """
    Camera interface for the Raspberry Pi camera.
    Configured for 10 FPS on Raspberry Pi by default.
    """
    
    def __init__(self, 
                 camera_type: str = "auto",
                 width: int = 640,
                 height: int = 480,
                 fps: int = 10):
        """
        Initialize camera handler.
        
        Args:
            camera_type: "pi_camera" or "auto" (Pi camera only)
            width: Frame width (default: 640)
            height: Frame height (default: 480)
            fps: Frames per second (default: 10)
        """
        self.width = width
        self.height = height
        self.fps = fps
        self.camera_type = None
        self.pi_camera = None
        self.is_initialized = False
        self.lock = Lock()
        
        self._init_camera(camera_type)
    
    def _init_camera(self, camera_type: str):
        """Initialize the Raspberry Pi camera."""
        if self.fps <= 0:
            raise ValueError("Camera FPS must be greater than zero")
        if camera_type not in ("auto", "pi_camera"):
            raise ValueError(f"Unsupported camera type: {camera_type}. Use 'pi_camera' or 'auto'.")
        if not PI_CAMERA_AVAILABLE:
            raise RuntimeError("❌ picamera2 not installed")
        if not CV2_AVAILABLE:
            raise RuntimeError("❌ OpenCV (cv2) is required to process Pi Camera frames")

        logger.info("🎥 Attempting Pi Camera initialization...")
        self.pi_camera = Picamera2()
        self.camera_type = "pi_camera"
        self._setup_pi_camera()
        self.is_initialized = True
        logger.info("✅ Pi Camera Module connected")
    
    def _setup_pi_camera(self):
        """Configure Pi Camera Module."""
        if self.pi_camera is None:
            return
        
        config = self.pi_camera.create_preview_configuration(
            main={"format": "RGB888", "size": (self.width, self.height)},
            controls={"FrameDurationLimits": (1_000_000 // self.fps,
                                               1_000_000 // self.fps)}
        )
        self.pi_camera.configure(config)
        self.pi_camera.start()
        
        # Allow camera to warm up
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
            "model": "Pi Camera Module",
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

def create_camera_or_exit(width: int = 640, height: int = 480, fps: int = 10):
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
        fps=10
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