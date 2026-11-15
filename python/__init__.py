# python/__init__.py
from .camera_handler import CameraHandler, create_camera_or_exit
from .alert_rules import should_alert, get_time_period
from .telegram_bot import start_telegram_bot, run_bot_in_background, run_alert_monitor_in_background
from .send_telegram_photo import (
    send_telegram_photo,
    send_telegram_message,
    send_telegram_alert,
    send_startup_message
)
from .robot_state import (
    RobotState,
    robot_state,
    get_robot_state,
    get_status_string,
    update_robot_state_from_monitor,
)
from .monitor import SecurityMonitor, monitor_camera, run_monitor_in_background
from .autonomy import PatrolWaypoint, AutonomousGuard, load_waypoints_from_yaml
from .voice_controller import voice_speak, voice_listen

from .detection_state import (
    # State variables
    person_detected, weapon_detected, fire_detected,
    person_count, weapon_count, fire_count,
    current_location,
    
    # Read/Write functions
    write_detection_state,
    read_detection_state,
    set_detection_flag,
    read_detection_flag,
    
    # Location functions
    set_current_location,
    read_current_location,
    update_location_from_waypoint,
    
    # Alert functions
    write_alert_trigger,
    read_alert_trigger,
    
    # Command functions
    write_command,
    read_command,
    
    # Status functions
    write_robot_status,
    read_robot_status,
    
    # Integration function
    update_state_from_monitor,
)




__all__ = [
   'CameraHandler',
    'create_camera_or_exit',
    'should_alert',
    'get_time_period',
    'start_telegram_bot',
    'run_bot_in_background',
    'run_alert_monitor_in_background',
    'send_telegram_photo',
    'send_telegram_message',
    'send_telegram_alert',
    'send_startup_message',
    'RobotState',
    'robot_state',
    'get_robot_state',
    'get_status_string',
    'update_robot_state_from_monitor',
    'PatrolWaypoint',
    'AutonomousGuard',
    'load_waypoints_from_yaml',
    'SecurityMonitor',
    'monitor_camera',
    'run_monitor_in_background',
    
    # Detection state
    'person_detected',
    'weapon_detected',
    'fire_detected',
    'person_count',
    'weapon_count',
    'fire_count',
    'current_location',
    'write_detection_state',
    'read_detection_state',
    'set_detection_flag',
    'read_detection_flag',
    'set_current_location',
    'read_current_location',
    'update_location_from_waypoint',
    'write_alert_trigger',
    'read_alert_trigger',
    'write_command',
    'read_command',
    'write_robot_status',
    'read_robot_status',
    'update_state_from_monitor',
]

# ============================================================
# Auto-start when imported (optional)
# ============================================================

def auto_start(location: str = "Security Zone"):
    """Auto-start the monitor."""
    import threading
    
    # Start Telegram bot
    bot_thread = run_bot_in_background()
    
    # Start alert monitor
    alert_thread = run_alert_monitor_in_background()
    
    # ============================================================
    # ADD YOUR CODE HERE! 👇
    # ============================================================
    
    # Start monitor
    monitor_thread = run_monitor_in_background(location)
    
    return {
        "bot": bot_thread,
        "alert": alert_thread,
        "monitor": monitor_thread
    }


def start_robot(location: str = "Main Gate"):
    """
    Start the robot with all components.
    This is the main entry point.
    """
    import threading
    import time
    
    # ============================================================
    # ADD YOUR CODE HERE! 👇
    # ============================================================
    
    # Start all components
    threads = auto_start(location)
    
    print(f"🤖 Robot started at: {location}")
    print("Press Ctrl+C to stop")
    
    # Keep running
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n👋 Shutting down...")