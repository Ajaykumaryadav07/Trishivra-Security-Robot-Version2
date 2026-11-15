#!/usr/bin/env python3
"""Capture a Pi Camera frame and send it to Telegram.

Usage: python3 scripts/send_startup_snapshot.py
"""
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(PROJECT_ROOT))

from python.camera_handler import CameraHandler
from python.send_telegram_photo import send_telegram_photo

try:
    cam = CameraHandler(camera_type='pi_camera', width=640, height=480, fps=6)
    ok, frame = cam.read()
    cam.release()
    if not ok or frame is None:
        print('Pi Camera failed to capture')
        raise SystemExit(3)

    import cv2
    path = '/tmp/robot_startup_snapshot.jpg'
    cv2.imwrite(path, frame)
    sent = send_telegram_photo(path, '📸 Startup snapshot from Pi Camera')
    print('Sent:', sent)
    sys.exit(0 if sent else 4)
except Exception as error:
    print('Pi Camera snapshot failed:', error)
    sys.exit(5)
