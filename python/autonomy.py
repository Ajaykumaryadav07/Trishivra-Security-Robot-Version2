"""
Autonomy controller for Security Robot V2.

This module implements a lightweight state machine, patrol planner,
vision/obstacle safety integration, and command generation for the robot.
"""

import time
import math
import logging
from typing import List, Dict, Optional

from python.robot_state import robot_state
from python.detection_state import update_state_from_monitor, set_current_location, write_command
from python.sensor_fusion import OdometryIntegrator, EncoderSimulator, IMUSimulator, Pose2D
from python.obstacle_safety import ObstacleSafety
from python.camera_handler import CameraHandler
from python.monitor import SecurityMonitor
from python.send_telegram_photo import send_telegram_alert

logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')


class PatrolWaypoint:
    def __init__(self, name: str, action: str = 'F', duration: float = 0.0, x: float = 0.0, y: float = 0.0):
        self.name = name
        self.action = action
        self.duration = duration
        self.x = x
        self.y = y

    def __repr__(self):
        return f"PatrolWaypoint(name={self.name}, action={self.action}, duration={self.duration}, x={self.x}, y={self.y})"


class AutonomousGuard:
    """Main autonomous robot controller."""

    def __init__(self, waypoints: Optional[List[PatrolWaypoint]] = None):
        self.state = 'IDLE'
        self.waypoints = waypoints or []
        self.current_waypoint_index = 0
        self.odom = OdometryIntegrator(wheel_base=0.16, meters_per_tick=0.001)
        self.imu = IMUSimulator()
        self.encoders = EncoderSimulator()
        self.safety = ObstacleSafety(stop_threshold=0.25)
        self.camera = None
        self.current_goal: Optional[PatrolWaypoint] = None
        self.last_alert_time = 0.0
        self.alert_interval = 10.0
        self.last_state_change = time.time()

        if self.waypoints:
            self.current_goal = self.waypoints[0]
            set_current_location(self.current_goal.name)
            robot_state.current_waypoint_index = 0
            robot_state.total_waypoints = len(self.waypoints)
            robot_state.current_location = self.current_goal.name
            robot_state.write_status()

    def load_camera(self, width: int = 320, height: int = 240, fps: int = 6):
        self.camera = CameraHandler(camera_type='auto', width=width, height=height, fps=fps)
        return self.camera

    def start_patrol(self):
        self.change_state('PATROL')

    def stop_patrol(self):
        self.change_state('IDLE')

    def change_state(self, new_state: str):
        if self.state != new_state:
            logger.info("State change: %s -> %s", self.state, new_state)
            self.state = new_state
            robot_state.update_state(new_state)
            self.last_state_change = time.time()

    def next_waypoint(self):
        if not self.waypoints:
            return
        self.current_waypoint_index = (self.current_waypoint_index + 1) % len(self.waypoints)
        self.current_goal = self.waypoints[self.current_waypoint_index]
        robot_state.current_waypoint_index = self.current_waypoint_index
        robot_state.current_location = self.current_goal.name
        self.current_goal.action = self.current_goal.action
        set_current_location(self.current_goal.name)
        robot_state.write_status()

    def get_current_goal(self) -> Optional[PatrolWaypoint]:
        return self.current_goal

    def plan_motion(self, pose: Pose2D) -> Dict[str, float]:
        if self.current_goal is None:
            return {'linear': 0.0, 'angular': 0.0}

        dx = self.current_goal.x - pose.x
        dy = self.current_goal.y - pose.y
        distance = (dx**2 + dy**2) ** 0.5
        target_theta = math.atan2(dy, dx)
        angle_error = self.normalize_angle(target_theta - pose.theta)

        linear = 0.0
        angular = 0.0

        if distance > 0.2:
            linear = min(0.3, distance * 0.4)
            angular = max(-0.6, min(0.6, angle_error * 1.8))
        elif abs(angle_error) > 0.2:
            angular = max(-0.6, min(0.6, angle_error * 1.8))
        else:
            linear = 0.0
            angular = 0.0

        return {
            'linear': linear,
            'angular': angular,
            'distance': distance,
            'angle_error': angle_error
        }

    def normalize_angle(self, angle: float) -> float:
        while angle > math.pi:
            angle -= 2 * math.pi
        while angle < -math.pi:
            angle += 2 * math.pi
        return angle

    def update_sensors(self, left_ticks: int, right_ticks: int, imu_yaw: float, proximity: Dict[str, float]):
        self.odom.update(left_ticks, right_ticks, imu_yaw)
        self.imu.set_yaw(imu_yaw)
        self.safety.update_readings(proximity)
        robot_state.update_pose(self.odom.pose.x, self.odom.pose.y, math.degrees(self.odom.pose.theta))
        robot_state.update_obstacle(self.safety.is_too_close(), self.safety.get_closest_distance())

    def get_current_pose(self) -> Pose2D:
        robot_state.read_status()
        return Pose2D(
            x=robot_state.x,
            y=robot_state.y,
            theta=math.radians(robot_state.theta)
        )

    def run(self, location: str = "Unknown", max_cycles: int = 0):
        """Run the autonomous guard loop without LiDAR."""
        if self.camera is None:
            self.load_camera()

        if self.current_goal is None and self.waypoints:
            self.current_goal = self.waypoints[0]
            set_current_location(self.current_goal.name)
            robot_state.current_location = self.current_goal.name
            robot_state.write_status()

        self.change_state('PATROL')
        robot_state.send_command('PATROL')

        monitor = SecurityMonitor()
        cycle = 0
        stopped_for_obstacle = False

        while self.state not in ('EMERGENCY_STOP', 'THREAT_DETECTED') and (max_cycles <= 0 or cycle < max_cycles):
            success, frame = self.camera.read()
            if not success or frame is None:
                time.sleep(0.1)
                continue

            detections = monitor.detect(frame)
            update_state_from_monitor(
                person_count=sum(1 for d in detections if d.get('class') == 'person'),
                weapon_count=sum(1 for d in detections if d.get('class') == 'weapon'),
                fire_count=sum(1 for d in detections if d.get('class') == 'fire'),
                location=robot_state.current_location
            )

            if robot_state.fire_detected or robot_state.weapon_detected:
                self.change_state('THREAT_DETECTED')
                robot_state.send_command('STOP')
                self.send_alert()
                break

            if robot_state.obstacle_detected:
                if self.state != 'EMERGENCY_STOP':
                    self.change_state('EMERGENCY_STOP')
                    robot_state.send_command('STOP')
                    stopped_for_obstacle = True
            elif stopped_for_obstacle:
                self.change_state('PATROL')
                robot_state.send_command('RESUME')
                stopped_for_obstacle = False

            pose = self.get_current_pose()
            motion = self.plan_motion(pose)

            if self.current_goal and motion['distance'] < 0.3:
                self.next_waypoint()

            if self.state == 'PATROL':
                robot_state.send_command('PATROL')

            time.sleep(1.0 / max(1, self.camera.fps))
            cycle += 1

        self.camera.release()
        if self.state == 'THREAT_DETECTED':
            logger.info('Autonomous guard stopped due to detected threat.')
        elif self.state == 'EMERGENCY_STOP':
            logger.info('Autonomous guard stopped due to obstacle.')
        else:
            logger.info('Autonomous guard loop ended.')

    def run_cycle(self, frame=None, detections=None):
        if self.state == 'IDLE':
            self.change_state('PATROL')

        if self.safety.is_too_close():
            self.change_state('EMERGENCY_STOP')

        if detections is not None:
            update_state_from_monitor(
                person_count=sum(1 for d in detections if d.get('class') == 'person'),
                weapon_count=sum(1 for d in detections if d.get('class') == 'weapon'),
                fire_count=sum(1 for d in detections if d.get('class') == 'fire'),
                location=robot_state.current_location
            )
            if robot_state.fire_detected or robot_state.weapon_detected:
                self.change_state('THREAT_DETECTED')
            elif robot_state.person_detected:
                self.change_state('THREAT_DETECTED')

        if self.state == 'EMERGENCY_STOP':
            return {'linear': 0.0, 'angular': 0.0, 'reason': 'Emergency stop'}

        if self.state == 'THREAT_DETECTED':
            self.send_alert()
            return {'linear': 0.0, 'angular': 0.0, 'reason': 'Threat detected'}

        if self.state == 'PATROL' and self.current_goal:
            pose = self.odom.pose
            motion = self.plan_motion(pose)
            if motion['distance'] < 0.3:
                self.next_waypoint()
                motion = {'linear': 0.0, 'angular': 0.0, 'reason': 'Waypoint reached'}
            if self.safety.is_too_close():
                motion['linear'] = 0.0
                motion['angular'] = 0.0
                self.change_state('OBSTACLE_AVOID')
            return motion

        return {'linear': 0.0, 'angular': 0.0, 'reason': 'Idle or no goal'}

    def send_alert(self):
        if time.time() - self.last_alert_time < self.alert_interval:
            return
        message = f"Threat detected at {robot_state.current_location}. Persons: {robot_state.person_count}, Weapons: {robot_state.weapon_count}, Fires: {robot_state.fire_count}"
        send_telegram_alert(alert_type='AUTONOMOUS_THREAT', message=message)
        self.last_alert_time = time.time()

    def get_status(self) -> Dict[str, any]:
        return {
            'state': self.state,
            'location': robot_state.current_location,
            'pose': self.odom.pose,
            'obstacle': self.safety.last_readings,
            'goal': self.current_goal.name if self.current_goal else None
        }


def load_waypoints_from_yaml(path: str) -> List[PatrolWaypoint]:
    import yaml

    try:
        with open(path, 'r', encoding='utf-8') as f:
            data = yaml.safe_load(f)
    except Exception as e:
        logger.error('Failed to load waypoints: %s', e)
        return []

    items = []
    for entry in data.get('waypoints', []):
        items.append(PatrolWaypoint(
            name=entry.get('name', 'unknown'),
            action=entry.get('action', 'F'),
            duration=float(entry.get('duration', 0.0)),
            x=float(entry.get('x', 0.0)),
            y=float(entry.get('y', 0.0))
        ))
    return items
