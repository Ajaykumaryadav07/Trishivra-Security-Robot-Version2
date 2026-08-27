#!/usr/bin/env python3
"""
Capture one frame using the project's camera handler (or cv2 fallback)
and send it to Telegram using the project's send_telegram_photo helper.

Usage: python3 scripts/send_startup_snapshot.py
"""
import os
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(PROJECT_ROOT))

from python.send_telegram_photo import send_telegram_photo

# Try to use the project's CameraHandler
try:
    from python.camera_handler import CameraHandler
    cam = CameraHandler(camera_type='auto', width=640, height=480, fps=6)
    ok, frame = cam.read()
    if ok and frame is not None:
        import cv2
        path = '/tmp/robot_startup_snapshot.jpg'
        cv2.imwrite(path, frame)
        cam.release()
        print('Captured frame to', path)
        sent = send_telegram_photo(path, '📸 Startup snapshot from robot')
        print('Sent:', sent)
        sys.exit(0 if sent else 2)
    else:
        cam.release()
        print('CameraHandler failed to capture')
except Exception as e:
    print('CameraHandler not available or failed:', e)

# Fallback: try OpenCV directly
try:
    import cv2
    cap = cv2.VideoCapture(int(os.getenv('USB_CAMERA_INDEX', '0')))
    ret, frame = cap.read()
    cap.release()
    if not ret or frame is None:
        print('OpenCV capture failed')
        raise SystemExit(3)
    path = '/tmp/robot_startup_snapshot.jpg'
    cv2.imwrite(path, frame)
    sent = send_telegram_photo(path, '📸 Startup snapshot from robot (cv2)')
    print('Sent:', sent)
    sys.exit(0 if sent else 4)
except Exception as e:
    print('OpenCV fallback failed:', e)
    sys.exit(5)
