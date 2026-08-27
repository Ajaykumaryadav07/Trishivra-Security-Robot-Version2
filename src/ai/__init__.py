# src/ai/__init__.py
"""
AI Module for Trishivra Security Robot V2

Provides:
- YOLO object detection with OpenVINO
- Person, Weapon, Fire detection
"""

from .yolo_openvino import YOLOOpenVINO, convert_yolo_to_openvino

__all__ = [
    'YOLOOpenVINO',
    'convert_yolo_to_openvino',
]