"""
Common constants and file-path helpers for Security Robot V2 Python modules.

This module centralizes temporary file locations and config paths so the
project works more consistently across platforms (Linux, macOS, Windows).
"""

import os
import tempfile
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]

# Use system temp directory (cross-platform) instead of hard-coded /tmp
TMP_DIR = Path(tempfile.gettempdir())

# Shared file names used for inter-process communication with C++ main
DETECTION_FILE = str(TMP_DIR / "detection_state.json")
ALERT_FILE = str(TMP_DIR / "alert_trigger.txt")
LOCATION_FILE = str(TMP_DIR / "current_location.txt")
COMMAND_FILE = str(TMP_DIR / "robot_command.txt")
STATUS_FILE = str(TMP_DIR / "robot_status.json")
FLAG_FILE = str(TMP_DIR / "alert_flag.txt")

# Config files inside project
CONFIG_FILE = PROJECT_ROOT / "config" / "robot_settings.json"
WAYPOINTS_FILE = PROJECT_ROOT / "config" / "waypoints.yaml"
LEGACY_WAYPOINTS_FILE = PROJECT_ROOT / "config" / "waypoints.json"

if not WAYPOINTS_FILE.exists() and LEGACY_WAYPOINTS_FILE.exists():
    WAYPOINTS_FILE = LEGACY_WAYPOINTS_FILE


def ensure_tmp_dir_exists() -> None:
	"""Create the tmp dir if it does not exist (no-op on most systems)."""
	try:
		TMP_DIR.mkdir(parents=True, exist_ok=True)
	except Exception:
		# Best-effort: ignore errors here; callers should handle IO errors
		pass


ensure_tmp_dir_exists()
