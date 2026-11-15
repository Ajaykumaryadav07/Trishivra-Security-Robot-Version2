from pathlib import Path

from python import autonomy


def test_configured_patrol_route_loads_action_waypoints():
    route_path = Path(autonomy.__file__).resolve().parents[1] / "config" / "waypoints.yaml"
    waypoints = autonomy.load_waypoints_from_yaml(str(route_path))

    assert len(waypoints) == 17
    assert waypoints[0].action == "F"
    assert waypoints[0].duration == 8.0
    assert waypoints[7].action == "S"
    assert waypoints[-1].action == "S"


def test_patrol_generates_motion_and_stops_for_obstacle(monkeypatch):
    monkeypatch.setattr(autonomy.robot_state, "write_status", lambda: True)
    monkeypatch.setattr(autonomy.robot_state, "update_state", lambda state: None)
    monkeypatch.setattr(autonomy, "set_current_location", lambda location: None)

    guard = autonomy.AutonomousGuard([
        autonomy.PatrolWaypoint(name="Test goal", x=1.0, y=0.0)
    ])

    motion = guard.run_cycle()
    assert motion["linear"] > 0

    guard.update_sensors(
        left_ticks=0,
        right_ticks=0,
        imu_yaw=0.0,
        proximity={"left": 1.0, "center": 0.1, "right": 1.0},
    )
    stopped = guard.run_cycle()

    assert guard.state == "EMERGENCY_STOP"
    assert stopped["linear"] == 0.0
    assert stopped["angular"] == 0.0
