#!/usr/bin/env python3
# run_robot.py
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from python.monitor import monitor_camera

if __name__ == "__main__":
    monitor_camera("Main Gate")