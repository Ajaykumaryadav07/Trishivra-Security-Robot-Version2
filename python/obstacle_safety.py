"""
Obstacle safety layer for Security Robot V2.

This module tracks simple proximity and ultrasonic/IR alerts and
translates them into emergency stop decisions.
"""

import logging
from dataclasses import dataclass
from typing import Dict

logger = logging.getLogger(__name__)


@dataclass
class ProximityReading:
    left: float = float('inf')
    center: float = float('inf')
    right: float = float('inf')


class ObstacleSafety:
    """Simple safety helper using proximity and short-range sensors."""

    def __init__(self, stop_threshold: float = 0.25):
        self.stop_threshold = stop_threshold
        self.last_readings = ProximityReading()

    def update_readings(self, readings: Dict[str, float]) -> ProximityReading:
        self.last_readings = ProximityReading(
            left=readings.get('left', float('inf')),
            center=readings.get('center', float('inf')),
            right=readings.get('right', float('inf'))
        )
        logger.debug("Proximity readings updated: %s", self.last_readings)
        return self.last_readings

    def is_too_close(self) -> bool:
        too_close = (
            self.last_readings.left <= self.stop_threshold or
            self.last_readings.center <= self.stop_threshold or
            self.last_readings.right <= self.stop_threshold
        )
        if too_close:
            logger.warning("🚨 Obstacle too close: %s", self.last_readings)
        return too_close

    def get_closest_distance(self) -> float:
        return min(self.last_readings.left, self.last_readings.center, self.last_readings.right)

    def get_action_for_obstacle(self) -> str:
        if self.last_readings.center <= self.stop_threshold:
            return 'STOP'
        if self.last_readings.left <= self.stop_threshold:
            return 'TURN_RIGHT'
        if self.last_readings.right <= self.stop_threshold:
            return 'TURN_LEFT'
        return 'CLEAR'
