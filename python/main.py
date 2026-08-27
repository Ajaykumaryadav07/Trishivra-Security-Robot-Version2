#!/usr/bin/env python3
"""
Main entry point for Security Robot V2 (Python side)
"""

import sys
import time
import threading
from pathlib import Path

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

from python.autonomy import AutonomousGuard, load_waypoints_from_yaml
from python.telegram_bot import start_telegram_bot, run_alert_monitor_in_background
from python.send_telegram_photo import send_startup_message


def main():
    """Main entry point."""
    print("🔱 TRISHIVRA Security Robot V2 (Python)")
    print("=" * 40)

    # Start Telegram bot in background
    print("🤖 Starting Telegram bot...")
    telegram_thread = threading.Thread(target=start_telegram_bot, daemon=True)
    telegram_thread.start()

    # Start alert monitor
    print("📡 Starting alert monitor...")
    alert_thread = run_alert_monitor_in_background()

    # Start autonomous guard with waypoints from config
    waypoints_file = PROJECT_ROOT / "config" / "waypoints.yaml"
    waypoints = []
    if waypoints_file.exists():
        waypoints = load_waypoints_from_yaml(str(waypoints_file))

    guard = AutonomousGuard(waypoints)
    guard.load_camera(width=640, height=480, fps=6)

    # Send startup notification plus one camera snapshot after camera is active
    send_startup_message()

    guard_thread = threading.Thread(target=guard.run, args=("Main Gate",), daemon=True)
    guard_thread.start()
    print("🤖 Autonomous guard thread started")
    
    # OR Option 2: Class instance
    # monitor = SecurityMonitor()
    # monitor.run("Admin Block")
    
    # OR Option 3: Run in background
    # from python.monitor import run_monitor_in_background
    # monitor_thread = run_monitor_in_background("Security Zone")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n👋 Shutting down...")