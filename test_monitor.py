#!/usr/bin/env python3
"""
Test script for Security Monitor
"""

import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(PROJECT_ROOT))

from python.monitor import SecurityMonitor, monitor_camera


def test_monitor():
    """Test the monitor."""
    print("🧪 Testing Security Monitor...")
    print("=" * 40)
    
    # ============================================================
    # ADD YOUR CODE HERE! 👇
    # ============================================================
    
    # Option 1: Direct run
    print("📷 Option 1: Direct run")
    monitor_camera("Test Location")
    
    # Option 2: Class instance (uncomment to test)
    # print("📷 Option 2: Class instance")
    # monitor = SecurityMonitor()
    # monitor.run("Admin Block")


if __name__ == "__main__":
    try:
        test_monitor()
    except KeyboardInterrupt:
        print("\n👋 Test stopped")