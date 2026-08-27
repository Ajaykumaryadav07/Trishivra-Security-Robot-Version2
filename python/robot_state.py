"""
Robot State Manager for Security Robot V2

Shared state between all Python modules:
- Monitor (detection)
- Telegram Bot (commands)
- C++ main (via file communication)

Centralizes robot state, configuration, and status.
"""

import os
import json
import time
import logging
from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, Any

from python.constants import STATUS_FILE, DETECTION_FILE, ALERT_FILE, LOCATION_FILE, COMMAND_FILE, FLAG_FILE

# Setup logging
logger = logging.getLogger(__name__)

# ============================================================
# Paths
# ============================================================

PROJECT_ROOT = Path(__file__).resolve().parents[1]
CONFIG_FILE = PROJECT_ROOT / "config" / "robot_settings.json"
WAYPOINTS_FILE = PROJECT_ROOT / "config" / "waypoints.json"

# ============================================================
# Robot State Class
# ============================================================

class RobotState:
    """
    Central robot state manager.
    Holds all robot state variables and provides file-based communication.
    """
    
    def __init__(self):
        """Initialize robot state with default values."""
        
        # ============================================================
        # State Variables
        # ============================================================
        
        # Control state
        self.paused = False
        self.running = True
        self.current_state = "IDLE"  # IDLE, PATROL, MAPPING, THREAT, EMERGENCY
        
        # Position (from C++ main)
        self.x = 0.0
        self.y = 0.0
        self.theta = 0.0
        
        # Battery
        self.battery_voltage = 12.4
        self.battery_low = False
        
        # Detection (from monitor.py)
        self.person_count = 0
        self.weapon_count = 0
        self.fire_count = 0
        self.person_detected = False
        self.weapon_detected = False
        self.fire_detected = False
        
        # Location
        self.current_location = "Unknown"
        self.current_waypoint_index = 0
        self.total_waypoints = 0
        
        # Time tracking
        self.last_detection_time = None
        self.last_alert_time = 0
        self.start_time = datetime.now()
        self.uptime_seconds = 0
        
        # System metrics (from C++ main)
        self.cpu_usage = 0.0
        self.temperature = 0.0
        self.fps = 0.0
        
        # Obstacles
        self.obstacle_detected = False
        self.obstacle_distance = 0.0
        
        # Messages
        self.last_message = ""
        self.last_esp32_message = ""
        
        # Commands
        self.pending_command = None
        self.command_value = ""
        
        # ============================================================
        # File Paths
        # ============================================================
        
        self.STATUS_FILE = STATUS_FILE
        self.DETECTION_FILE = DETECTION_FILE
        self.COMMAND_FILE = COMMAND_FILE
        self.ALERT_FILE = ALERT_FILE
        self.LOCATION_FILE = LOCATION_FILE
        self.FLAG_FILE = FLAG_FILE
        
        # Load config
        self.config = self.load_config()
        self.waypoints = self.load_waypoints()
        
        # Write initial status
        self.write_status()
    
    # ============================================================
    # Configuration Loading
    # ============================================================
    
    def load_config(self) -> Dict[str, Any]:
        """Load configuration from JSON file."""
        try:
            if CONFIG_FILE.exists():
                with open(CONFIG_FILE, 'r') as f:
                    return json.load(f)
        except Exception as e:
            logger.error(f"❌ Failed to load config: {e}")
        return {}
    
    def load_waypoints(self) -> list:
        """Load waypoints from JSON file."""
        try:
            if WAYPOINTS_FILE.exists():
                with open(WAYPOINTS_FILE, 'r') as f:
                    return json.load(f)
        except Exception as e:
            logger.error(f"❌ Failed to load waypoints: {e}")
        return []
    
    # ============================================================
    # Status Read/Write (for C++ main)
    # ============================================================
    
    def write_status(self) -> bool:
        """Write current status to file for C++ main and Telegram."""
        try:
            data = {
                "state": self.current_state,
                "x": self.x,
                "y": self.y,
                "theta": self.theta,
                "battery": self.battery_voltage,
                "battery_low": self.battery_low,
                "person_count": self.person_count,
                "weapon_count": self.weapon_count,
                "fire_count": self.fire_count,
                "person_detected": self.person_detected,
                "weapon_detected": self.weapon_detected,
                "fire_detected": self.fire_detected,
                "location": self.current_location,
                "waypoint": self.current_waypoint_index,
                "total_waypoints": self.total_waypoints,
                "cpu": self.cpu_usage,
                "temperature": self.temperature,
                "fps": self.fps,
                "paused": self.paused,
                "obstacle": self.obstacle_detected,
                "uptime": self.uptime_seconds,
                "last_message": self.last_message,
                "timestamp": time.time(),
                "datetime": datetime.now().isoformat()
            }
            
            with open(self.STATUS_FILE, 'w') as f:
                json.dump(data, f, indent=2)
            
            return True
        except Exception as e:
            logger.error(f"❌ Failed to write status: {e}")
            return False
    
    def read_status(self) -> Dict[str, Any]:
        """Read status from file (written by C++ main)."""
        try:
            if os.path.exists(self.STATUS_FILE):
                with open(self.STATUS_FILE, 'r') as f:
                    data = json.load(f)
                    
                    # Update local state from file
                    self.current_state = data.get("state", "UNKNOWN")
                    self.x = data.get("x", 0.0)
                    self.y = data.get("y", 0.0)
                    self.theta = data.get("theta", 0.0)
                    self.battery_voltage = data.get("battery", 12.4)
                    self.battery_low = data.get("battery_low", False)
                    self.cpu_usage = data.get("cpu", 0.0)
                    self.temperature = data.get("temperature", 0.0)
                    self.fps = data.get("fps", 0.0)
                    self.paused = data.get("paused", False)
                    self.obstacle_detected = data.get("obstacle", False)
                    
                    return data
        except Exception as e:
            logger.error(f"❌ Failed to read status: {e}")
        
        return {
            "state": "UNKNOWN",
            "x": 0, "y": 0, "theta": 0,
            "battery": 0, "cpu": 0, "temperature": 0,
            "fps": 0, "paused": False, "obstacle": False
        }
    
    # ============================================================
    # Detection State (from monitor.py)
    # ============================================================
    
    def write_detection_state(self) -> bool:
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
                "timestamp": time.time(),
                "datetime": datetime.now().isoformat()
            }
            
            with open(self.DETECTION_FILE, 'w') as f:
                json.dump(data, f, indent=2)
            
            # Also write simple flag for legacy
            with open(self.FLAG_FILE, 'w') as f:
                f.write(str(1 if self.person_detected else 0))
            
            return True
        except Exception as e:
            logger.error(f"❌ Failed to write detection state: {e}")
            return False
    
    def read_detection_state(self) -> Dict[str, Any]:
        """Read detection state from file."""
        try:
            if os.path.exists(self.DETECTION_FILE):
                with open(self.DETECTION_FILE, 'r') as f:
                    data = json.load(f)
                    
                    # Update local state
                    self.person_count = data.get("person_count", 0)
                    self.weapon_count = data.get("weapon_count", 0)
                    self.fire_count = data.get("fire_count", 0)
                    self.person_detected = data.get("person_detected", False)
                    self.weapon_detected = data.get("weapon_detected", False)
                    self.fire_detected = data.get("fire_detected", False)
                    
                    return data
        except Exception as e:
            logger.error(f"❌ Failed to read detection state: {e}")
        
        return {
            "person_count": 0,
            "weapon_count": 0,
            "fire_count": 0,
            "person_detected": False,
            "weapon_detected": False,
            "fire_detected": False,
            "location": "Unknown"
        }
    
    # ============================================================
    # Commands (Telegram -> C++)
    # ============================================================
    
    def send_command(self, command: str, value: str = "") -> bool:
        """Send command to C++ main via file."""
        try:
            with open(self.COMMAND_FILE, 'w') as f:
                if value:
                    f.write(f"{command} {value}\n")
                else:
                    f.write(f"{command}\n")
            
            self.pending_command = command
            self.command_value = value
            logger.info(f"📤 Command sent: {command} {value}")
            return True
        except Exception as e:
            logger.error(f"❌ Failed to send command: {e}")
            return False
    
    def read_command(self) -> Optional[Dict[str, str]]:
        """Read command from file (for C++ main)."""
        try:
            if os.path.exists(self.COMMAND_FILE):
                with open(self.COMMAND_FILE, 'r') as f:
                    content = f.read().strip()
                
                # Clear file
                os.remove(self.COMMAND_FILE)
                
                if content:
                    parts = content.split()
                    if len(parts) >= 1:
                        cmd = parts[0]
                        val = parts[1] if len(parts) > 1 else ""
                        return {"command": cmd, "value": val}
        except Exception as e:
            logger.error(f"❌ Failed to read command: {e}")
        
        return None
    
    # ============================================================
    # Alerts (C++ -> Telegram)
    # ============================================================
    
    def write_alert(self, alert_type: str, message: str, photo_path: str = "") -> bool:
        """Write alert to file for Telegram."""
        try:
            lines = [
                alert_type,
                f"Message: {message}",
                f"Photo: {photo_path}",
                f"Time: {datetime.now().isoformat()}"
            ]
            
            with open(self.ALERT_FILE, 'w') as f:
                f.write('\n'.join(lines))
            
            return True
        except Exception as e:
            logger.error(f"❌ Failed to write alert: {e}")
            return False
    
    def read_alert(self) -> Optional[Dict[str, Any]]:
        """Read alert from file."""
        try:
            if os.path.exists(self.ALERT_FILE):
                with open(self.ALERT_FILE, 'r') as f:
                    content = f.read().strip()
                
                # Clear file
                os.remove(self.ALERT_FILE)
                
                if content:
                    lines = content.split('\n')
                    data = {"type": lines[0] if lines else "UNKNOWN"}
                    
                    for line in lines[1:]:
                        if ': ' in line:
                            key, value = line.split(': ', 1)
                            data[key] = value
                    
                    return data
        except Exception as e:
            logger.error(f"❌ Failed to read alert: {e}")
        
        return None
    
    # ============================================================
    # Location Management
    # ============================================================
    
    def set_location(self, location: str) -> bool:
        """Set current location."""
        self.current_location = location
        
        try:
            with open(self.LOCATION_FILE, 'w', encoding='utf-8') as f:
                f.write(str(location))
            return True
        except Exception as e:
            logger.error(f"❌ Failed to write location: {e}")
            return False
    
    def get_location(self) -> str:
        """Get current location."""
        try:
            if os.path.exists(self.LOCATION_FILE):
                with open(self.LOCATION_FILE, 'r', encoding='utf-8') as f:
                    location = f.read().strip()
                    self.current_location = location
                    return location
        except Exception as e:
            logger.error(f"❌ Failed to read location: {e}")
        
        return self.current_location
    
    # ============================================================
    # Update Methods
    # ============================================================
    
    def update_uptime(self):
        """Update uptime in seconds."""
        self.uptime_seconds = int((datetime.now() - self.start_time).total_seconds())
    
    def update_detection(
        self,
        person_count: int = 0,
        weapon_count: int = 0,
        fire_count: int = 0,
        location: str = None
    ):
        """Update detection state from monitor."""
        self.person_count = person_count
        self.weapon_count = weapon_count
        self.fire_count = fire_count
        self.person_detected = person_count > 0
        self.weapon_detected = weapon_count > 0
        self.fire_detected = fire_count > 0
        
        if location:
            self.set_location(location)
        
        if self.person_detected or self.weapon_detected or self.fire_detected:
            self.last_detection_time = datetime.now()
        
        # Write to file
        self.write_detection_state()
        self.write_status()
    
    def update_pose(self, x: float, y: float, theta: float):
        """Update robot pose."""
        self.x = x
        self.y = y
        self.theta = theta
        self.write_status()
    
    def update_battery(self, voltage: float):
        """Update battery voltage."""
        self.battery_voltage = voltage
        self.battery_low = voltage < 11.5
        self.write_status()
    
    def update_state(self, state: str):
        """Update robot state."""
        self.current_state = state
        self.write_status()
    
    def update_metrics(self, cpu: float, temp: float, fps: float):
        """Update system metrics."""
        self.cpu_usage = cpu
        self.temperature = temp
        self.fps = fps
        self.write_status()
    
    def update_obstacle(self, detected: bool, distance: float = 0.0):
        """Update obstacle state."""
        self.obstacle_detected = detected
        self.obstacle_distance = distance
        self.write_status()
    
    # ============================================================
    # Status String (for Telegram)
    # ============================================================
    
    def get_status_string(self) -> str:
        """Get formatted status string for Telegram."""
        # Read latest status from C++
        self.read_status()
        self.read_detection_state()
        self.update_uptime()
        
        state_emoji = {
            "IDLE": "IDLE",
            "PATROL": "PATROL",
            "MAPPING": "MAPPING",
            "THREAT_DETECTED": "THREAT_DETECTED",
            "EMERGENCY_STOP": "EMERGENCY_STOP",
            "RETURN_HOME": "RETURN_HOME",
            "OBSTACLE_AVOID": "OBSTACLE_AVOID",
            "PAUSED": "PAUSED",
            "SHUTDOWN": "SHUTDOWN"
        }
        
        emoji = state_emoji.get(self.current_state, "UNKNOWN")
        
        status = (
            f"{emoji} **Robot Status**\n\n"
            f"State: {self.current_state}\n"
            f"Position: ({self.x:.2f}, {self.y:.2f})\n"
            f"Heading: {self.theta:.2f} deg\n"
            f"Battery: {self.battery_voltage:.1f}V {'LOW' if self.battery_low else 'OK'}\n"
            f"Persons: {self.person_count}\n"
            f"Weapons: {self.weapon_count}\n"
            f"Fires: {self.fire_count}\n"
            f"Location: {self.current_location}\n"
            f"CPU: {self.cpu_usage:.1f}%\n"
            f"Temp: {self.temperature:.1f}C\n"
            f"FPS: {self.fps:.1f}\n"
            f"Paused: {'Yes' if self.paused else 'No'}\n"
            f"Obstacle: {'Yes' if self.obstacle_detected else 'No'}\n"
            f"Uptime: {self.uptime_seconds // 3600}h {self.uptime_seconds // 60 % 60}m"
        )
        
        return status
    
    # ============================================================
    # Reset
    # ============================================================
    
    def reset(self):
        """Reset all state variables."""
        self.paused = False
        self.running = True
        self.current_state = "IDLE"
        self.x = 0.0
        self.y = 0.0
        self.theta = 0.0
        self.person_count = 0
        self.weapon_count = 0
        self.fire_count = 0
        self.person_detected = False
        self.weapon_detected = False
        self.fire_detected = False
        self.obstacle_detected = False
        self.current_location = "Unknown"
        self.last_message = "State reset"
        
        self.write_status()
        self.write_detection_state()
        
        logger.info("🔄 Robot state reset")


# ============================================================
# Global Instance
# ============================================================

robot_state = RobotState()


# ============================================================
# Helper Functions (for backwards compatibility)
# ============================================================

def get_robot_state() -> RobotState:
    """Get global robot state instance."""
    return robot_state

def get_status_string() -> str:
    """Get formatted status string."""
    return robot_state.get_status_string()

def update_robot_state_from_monitor(person_count, weapon_count, fire_count, location):
    """Update state from monitor."""
    robot_state.update_detection(person_count, weapon_count, fire_count, location)

# ============================================================
# Test
# ============================================================

if __name__ == "__main__":
    print("🧪 Testing robot_state.py...")
    print("=" * 40)
    
    # Test status
    status = robot_state.get_status_string()
    print(status)
    
    # Test update
    print("\n📝 Updating state...")
    robot_state.update_detection(3, 1, 0, "Main Gate")
    robot_state.update_state("PATROL")
    robot_state.update_pose(1.5, 2.3, 0.8)
    robot_state.update_battery(11.8)
    
    # Print updated status
    print("\n📊 Updated status:")
    print(robot_state.get_status_string())
    
    print("\n✅ All tests complete!")