"""
Detection State Manager for Security Robot V2

Shared state between:
- Python monitor (detection)
- C++ main (navigation/control)
- Telegram bot (status/commands)

Uses file-based communication via /tmp/ directory.
"""

import os
import json
import time
import logging
from datetime import datetime
from pathlib import Path
from typing import Optional, Dict, Any

from python.constants import DETECTION_FILE, ALERT_FILE, LOCATION_FILE, COMMAND_FILE, STATUS_FILE, FLAG_FILE

# Setup logging
logger = logging.getLogger(__name__)

# ============================================================
# Global State Variables (Python side)
# ============================================================

# Detection flags
person_detected = False
weapon_detected = False
fire_detected = False
person_count = 0
weapon_count = 0
fire_count = 0

# Location
current_location = "Unknown"

# Time
last_detection_time = None
last_alert_time = 0

# System state
paused = False
running = True

# ============================================================
# Read/Write Detection State
# ============================================================

def write_detection_state(
    person_count: int = 0,
    weapon_count: int = 0,
    fire_count: int = 0,
    location: str = "Unknown",
    person_detected: bool = False,
    weapon_detected: bool = False,
    fire_detected: bool = False
) -> bool:
    """
    Write detection state to file for C++ main.
    
    Args:
        person_count: Number of persons detected
        weapon_count: Number of weapons detected
        fire_count: Number of fires detected
        location: Current location/waypoint
        person_detected: True if any person detected
        weapon_detected: True if any weapon detected
        fire_detected: True if any fire detected
    
    Returns:
        True if successful, False otherwise
    """
    try:
        data = {
            "person_count": person_count,
            "weapon_count": weapon_count,
            "fire_count": fire_count,
            "person_detected": person_detected,
            "weapon_detected": weapon_detected,
            "fire_detected": fire_detected,
            "location": location,
            "timestamp": time.time(),
            "datetime": datetime.now().isoformat()
        }
        
        with open(DETECTION_FILE, 'w') as f:
            json.dump(data, f, indent=2)
        
        return True
    except Exception as e:
        logger.error(f"❌ Failed to write detection state: {e}")
        return False


def read_detection_state() -> Dict[str, Any]:
    """
    Read detection state from file (written by Python monitor).
    
    Returns:
        Dictionary with detection data
    """
    try:
        if os.path.exists(DETECTION_FILE):
            with open(DETECTION_FILE, 'r') as f:
                return json.load(f)
        else:
            return {
                "person_count": 0,
                "weapon_count": 0,
                "fire_count": 0,
                "person_detected": False,
                "weapon_detected": False,
                "fire_detected": False,
                "location": "Unknown",
                "timestamp": 0
            }
    except Exception as e:
        logger.error(f"❌ Failed to read detection state: {e}")
        return {
            "person_count": 0,
            "weapon_count": 0,
            "fire_count": 0,
            "person_detected": False,
            "weapon_detected": False,
            "fire_detected": False,
            "location": "Error",
            "timestamp": 0
        }


def set_detection_flag(status: int) -> bool:
    """
    Set simple detection flag (0 or 1) for legacy systems.
    
    Args:
        status: 1 = person detected, 0 = no person
    
    Returns:
        True if successful
    """
    try:
        os.makedirs(os.path.dirname(FLAG_FILE), exist_ok=True)
        with open(FLAG_FILE, 'w') as f:
            f.write(str(int(status)))
        return True
    except Exception as e:
        logger.error(f"❌ Failed to write flag: {e}")
        return False


def read_detection_flag() -> int:
    """
    Read simple detection flag.
    
    Returns:
        1 = person detected, 0 = no person, -1 = error
    """
    try:
        if os.path.exists(FLAG_FILE):
            with open(FLAG_FILE, 'r') as f:
                return int(f.read().strip())
        return 0
    except Exception as e:
        logger.error(f"❌ Failed to read flag: {e}")
        return -1

# ============================================================
# Location Management
# ============================================================

def set_current_location(location: str) -> bool:
    """
    Set current location for detection rules.
    
    Args:
        location: Location/waypoint name
    
    Returns:
        True if successful
    """
    global current_location
    current_location = location
    
    try:
        os.makedirs(os.path.dirname(LOCATION_FILE), exist_ok=True)
        with open(LOCATION_FILE, 'w', encoding='utf-8') as f:
            f.write(str(location))
        return True
    except Exception as e:
        logger.error(f"❌ Failed to write location: {e}")
        return False


def read_current_location() -> str:
    """
    Read current location from file.
    
    Returns:
        Location string or empty string
    """
    global current_location
    
    try:
        if os.path.exists(LOCATION_FILE):
            with open(LOCATION_FILE, 'r', encoding='utf-8') as f:
                location = f.read().strip()
                current_location = location
                return location
    except Exception as e:
        logger.error(f"❌ Failed to read location: {e}")
    
    return current_location


def update_location_from_waypoint(waypoint_name: str) -> bool:
    """
    Update location from waypoint name.
    This is called by the patrol system when reaching a waypoint.
    
    Args:
        waypoint_name: Name of the current waypoint
    
    Returns:
        True if successful
    """
    return set_current_location(waypoint_name)

# ============================================================
# Alert Management (C++ Integration)
# ============================================================

def write_alert_trigger(
    alert_type: str,
    message: str,
    photo_path: str = "",
    extra_data: Dict[str, Any] = None
) -> bool:
    """
    Write alert trigger for C++ main.
    
    Args:
        alert_type: Type of alert (PERSON_DETECTED, WEAPON_DETECTED, etc.)
        message: Alert message
        photo_path: Path to saved photo
        extra_data: Additional data to include
    
    Returns:
        True if successful
    """
    try:
        lines = [
            alert_type,
            f"Message: {message}",
            f"Photo: {photo_path}",
            f"Time: {datetime.now().isoformat()}"
        ]
        
        if extra_data:
            for key, value in extra_data.items():
                lines.append(f"{key}: {value}")
        
        with open(ALERT_FILE, 'w') as f:
            f.write('\n'.join(lines))
        
        return True
    except Exception as e:
        logger.error(f"❌ Failed to write alert: {e}")
        return False


def read_alert_trigger() -> Optional[Dict[str, Any]]:
    """
    Read alert trigger from file (for C++ main).
    
    Returns:
        Dictionary with alert data, or None if no alert
    """
    try:
        if os.path.exists(ALERT_FILE):
            with open(ALERT_FILE, 'r') as f:
                content = f.read().strip()
            
            # Clear file after reading
            os.remove(ALERT_FILE)
            
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
# Command Management (Telegram -> C++)
# ============================================================

def write_command(command: str, value: str = "") -> bool:
    """
    Write command for C++ main to execute.
    
    Args:
        command: Command name (STOP, RESUME, PATROL, etc.)
        value: Optional value for command
    
    Returns:
        True if successful
    """
    try:
        with open(COMMAND_FILE, 'w') as f:
            if value:
                f.write(f"{command} {value}\n")
            else:
                f.write(f"{command}\n")
        return True
    except Exception as e:
        logger.error(f"❌ Failed to write command: {e}")
        return False


def read_command() -> Optional[Dict[str, str]]:
    """
    Read command from file (for C++ main).
    
    Returns:
        Dictionary with command and value, or None
    """
    try:
        if os.path.exists(COMMAND_FILE):
            with open(COMMAND_FILE, 'r') as f:
                content = f.read().strip()
            
            # Clear file after reading
            os.remove(COMMAND_FILE)
            
            if content:
                parts = content.split()
                if len(parts) >= 1:
                    return {
                        "command": parts[0],
                        "value": parts[1] if len(parts) > 1 else ""
                    }
    except Exception as e:
        logger.error(f"❌ Failed to read command: {e}")
    
    return None

# ============================================================
# Status Management
# ============================================================

def write_robot_status(status_data: Dict[str, Any]) -> bool:
    """
    Write robot status for Telegram bot.
    
    Args:
        status_data: Dictionary with status data
    
    Returns:
        True if successful
    """
    try:
        with open(STATUS_FILE, 'w') as f:
            json.dump(status_data, f, indent=2)
        return True
    except Exception as e:
        logger.error(f"❌ Failed to write status: {e}")
        return False


def read_robot_status() -> Dict[str, Any]:
    """
    Read robot status from file (written by C++ main).
    
    Returns:
        Dictionary with status data
    """
    try:
        if os.path.exists(STATUS_FILE):
            with open(STATUS_FILE, 'r') as f:
                return json.load(f)
    except Exception as e:
        logger.error(f"❌ Failed to read status: {e}")
    
    return {
        "state": "UNKNOWN",
        "x": 0, "y": 0, "theta": 0,
        "battery": 0, "person_count": 0,
        "weapon_count": 0, "fire_count": 0,
        "cpu": 0, "fps": 0, "paused": False,
        "obstacle": False, "temperature": 0
    }

# ============================================================
# Integration with monitor.py
# ============================================================

def update_state_from_monitor(
    person_count: int,
    weapon_count: int,
    fire_count: int,
    location: str = "Unknown"
) -> bool:
    """
    Update detection state from monitor.
    This is the main function called by monitor.py.
    
    Args:
        person_count: Number of persons detected
        weapon_count: Number of weapons detected
        fire_count: Number of fires detected
        location: Current location
    
    Returns:
        True if successful
    """
    # Update global variables
    global person_detected, weapon_detected, fire_detected
    global current_location, last_detection_time
    
    person_detected = person_count > 0
    weapon_detected = weapon_count > 0
    fire_detected = fire_count > 0
    current_location = location
    
    if person_detected or weapon_detected or fire_detected:
        last_detection_time = datetime.now()
    
    # Write to file
    return write_detection_state(
        person_count=person_count,
        weapon_count=weapon_count,
        fire_count=fire_count,
        location=location,
        person_detected=person_detected,
        weapon_detected=weapon_detected,
        fire_detected=fire_detected
    )

# ============================================================
# Test Functions
# ============================================================

def test_all():
    """Test all state functions."""
    print("🧪 Testing detection_state.py...")
    print("=" * 40)
    
    # 1. Write detection state
    print("📝 Writing detection state...")
    write_detection_state(
        person_count=3,
        weapon_count=1,
        fire_count=0,
        location="Main Gate",
        person_detected=True,
        weapon_detected=True,
        fire_detected=False
    )
    
    # 2. Read detection state
    print("📖 Reading detection state...")
    state = read_detection_state()
    print(f"   {state}")
    
    # 3. Set location
    print("📍 Setting location...")
    set_current_location("Admin Block")
    
    # 4. Read location
    print("📍 Reading location...")
    location = read_current_location()
    print(f"   Location: {location}")
    
    # 5. Write alert
    print("🚨 Writing alert...")
    write_alert_trigger(
        alert_type="PERSON_DETECTED",
        message="Person detected in restricted zone",
        photo_path="/tmp/alert.jpg",
        extra_data={"Confidence": 0.85, "Count": 3}
    )
    
    # 6. Read alert
    print("🚨 Reading alert...")
    alert = read_alert_trigger()
    print(f"   {alert}")
    
    # 7. Write command
    print("📤 Writing command...")
    write_command("PATROL", "Main Gate")
    
    # 8. Read command
    print("📥 Reading command...")
    cmd = read_command()
    print(f"   {cmd}")
    
    print("\n✅ All tests complete!")

if __name__ == "__main__":
    test_all()