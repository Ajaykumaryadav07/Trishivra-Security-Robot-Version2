"""
Sensor fusion and odometry helpers for Security Robot V2.

This module provides a simple pose integrator for encoder + IMU based
odometry. It is designed as a light placeholder that can be extended
for real Pi encoder and IMU hardware later.
"""

import math
import logging
from dataclasses import dataclass

logger = logging.getLogger(__name__)


@dataclass
class Pose2D:
    x: float = 0.0
    y: float = 0.0
    theta: float = 0.0  # radians


class OdometryIntegrator:
    """Simple encoder + IMU odometry integrator."""

    def __init__(self, wheel_base: float = 0.16, meters_per_tick: float = 0.001):
        self.wheel_base = wheel_base
        self.meters_per_tick = meters_per_tick
        self.left_ticks = 0
        self.right_ticks = 0
        self.pose = Pose2D()

    def update(self, left_ticks: int, right_ticks: int, imu_theta: float) -> Pose2D:
        """Update pose from encoder tick counts and IMU heading."""
        delta_left = (left_ticks - self.left_ticks) * self.meters_per_tick
        delta_right = (right_ticks - self.right_ticks) * self.meters_per_tick
        self.left_ticks = left_ticks
        self.right_ticks = right_ticks

        distance = (delta_left + delta_right) / 2.0
        rotation = (delta_right - delta_left) / max(self.wheel_base, 1e-6)

        # Use IMU heading when available to reduce drift.
        new_theta = imu_theta
        dx = distance * math.cos(new_theta)
        dy = distance * math.sin(new_theta)

        self.pose = Pose2D(
            x=self.pose.x + dx,
            y=self.pose.y + dy,
            theta=new_theta
        )

        logger.debug(
            "Odometry update: dl=%s dr=%s dist=%s theta=%.3f -> x=%.3f y=%.3f",
            delta_left, delta_right, distance, new_theta, self.pose.x, self.pose.y
        )
        return self.pose

    def reset(self, x: float = 0.0, y: float = 0.0, theta: float = 0.0):
        self.pose = Pose2D(x, y, theta)
        self.left_ticks = 0
        self.right_ticks = 0


class IMUSimulator:
    """Lightweight IMU placeholder for yaw tracking."""

    def __init__(self):
        self.yaw = 0.0

    def set_yaw(self, yaw: float):
        self.yaw = yaw

    def read_yaw(self) -> float:
        return self.yaw


class EncoderSimulator:
    """Lightweight encoder placeholder for tick-based odometry."""

    def __init__(self):
        self.left_ticks = 0
        self.right_ticks = 0

    def advance(self, left_delta: int = 0, right_delta: int = 0):
        self.left_ticks += left_delta
        self.right_ticks += right_delta

    def read(self):
        return self.left_ticks, self.right_ticks
