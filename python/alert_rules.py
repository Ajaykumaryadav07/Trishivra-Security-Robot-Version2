"""
Alert Rules for Security Robot V2

Defines time-period classification and alert decision logic
used by the camera monitoring and Telegram alert systems.

Based on:
- Time of day (day/evening/night)
- Location (waypoint name)
- Detection counts (persons, vehicles, weapons, fire)
"""

from datetime import datetime
from typing import Tuple, List, Optional


# ============================================================
# Time Period Classification
# ============================================================

def get_time_period(dt: datetime = None) -> str:
    """
    Classify current time into day/evening/night.
    
    Args:
        dt: datetime object (defaults to now)
    
    Returns:
        'day', 'evening', or 'night'
    """
    if dt is None:
        dt = datetime.now()
    
    hour = dt.hour
    
    if 8 <= hour < 18:
        return 'day'
    elif 18 <= hour < 22:
        return 'evening'
    else:
        return 'night'


# ============================================================
# Location Helpers
# ============================================================

def location_matches(location: str, keywords: List[str]) -> bool:
    """
    Check if location string contains any of the given keywords (case-insensitive).
    
    Args:
        location: Location name (e.g., "Main Gate", "Admin Block")
        keywords: List of keywords to match
    
    Returns:
        True if any keyword is found in location
    """
    if not location:
        return False
    loc_lower = location.lower()
    return any(kw.lower() in loc_lower for kw in keywords)


def is_restricted_zone(location: str) -> bool:
    """Check if location is a restricted zone (bank, admin, etc.)"""
    restricted = ['bank', 'admin', 'office', 'server', 'lab', 'research']
    return location_matches(location, restricted)


def is_no_parking_zone(location: str) -> bool:
    """Check if location is a no-parking zone (fire lane, main gate, etc.)"""
    no_parking = ['fire lane', 'main gate', 'emergency', 'entrance', 'blocked']
    return location_matches(location, no_parking)


# ============================================================
# Main Alert Decision Function
# ============================================================

def should_alert(
    person_count: int,
    vehicle_count: int = 0,
    weapon_count: int = 0,
    fire_count: int = 0,
    location: str = "Unknown",
    dt: Optional[datetime] = None
) -> Tuple[bool, str]:
    """
    Decide whether an alert should be triggered based on current state.
    
    Args:
        person_count: Number of persons detected
        vehicle_count: Number of vehicles detected
        weapon_count: Number of weapons detected
        fire_count: Number of fire incidents detected
        location: Current waypoint/location name
        dt: datetime object (defaults to now)
    
    Returns:
        (alert_needed, reason)
    """
    if dt is None:
        dt = datetime.now()
    
    # ============================================================
    # HIGHEST PRIORITY: Life-threatening events
    # ============================================================
    
    # Fire is always an emergency
    if fire_count > 0:
        return True, f"🔥 FIRE detected! ({fire_count} incidents)"
    
    # Weapon detection is immediate threat
    if weapon_count > 0:
        return True, f"🔫 WEAPON detected! ({weapon_count} weapons)"
    
    # ============================================================
    # Crowd / Group Rules (any time)
    # ============================================================
    
    if person_count > 10:
        return True, f"👥 Large crowd detected ({person_count} people)"
    
    if person_count >= 3 and location and 'hostel' in location.lower():
        return True, f"🏠 Hostel group detected ({person_count} people in hostel)"
    
    # ============================================================
    # Time-Based Rules
    # ============================================================
    
    tp = get_time_period(dt)
    loc_lower = location.lower() if location else ""
    
    # Night: most sensitive
    if tp == 'night':
        if person_count > 0:
            return True, f"🌙 Person detected at night ({person_count}) in {location}"
        if vehicle_count > 0 and is_no_parking_zone(location):
            return True, f"🚗 Vehicle in no-parking zone at night ({vehicle_count})"
        return False, ""
    
    # Evening: moderate sensitivity
    if tp == 'evening':
        if is_restricted_zone(location) and person_count > 0:
            return True, f"🌆 Person in restricted zone in evening ({person_count})"
        if vehicle_count > 0 and is_no_parking_zone(location):
            return True, f"🚗 Vehicle in no-parking zone in evening ({vehicle_count})"
        if person_count > 5:
            return True, f"👥 Group of {person_count} in evening"
        return False, ""
    
    # Day: only specific triggers
    if tp == 'day':
        if is_restricted_zone(location) and person_count > 3:
            return True, f"☀️ Person crowd in restricted zone during day ({person_count})"
        if vehicle_count > 0 and is_no_parking_zone(location):
            return True, f"🚗 Vehicle in no-parking zone during day ({vehicle_count})"
        return False, ""
    
    # Fallback
    return False, ""


# ============================================================
# Utility: Check if alert is cooldown-worthy
# ============================================================

def is_high_priority_alert(reason: str) -> bool:
    """Check if alert reason indicates a high-priority event."""
    high_priority_keywords = [
        "FIRE", "WEAPON", "EMERGENCY", "CRITICAL", "DANGER"
    ]
    return any(kw in reason.upper() for kw in high_priority_keywords)


# ============================================================
# Simple Test
# ============================================================

if __name__ == "__main__":
    # Test cases
    test_cases = [
        (5, 0, 0, 0, "Main Gate", "2026-08-06 23:00:00"),  # Night person
        (0, 1, 0, 0, "Fire Lane", "2026-08-06 20:00:00"),  # Evening vehicle
        (2, 0, 0, 0, "Admin Block", "2026-08-06 14:00:00"),  # Day restricted zone
        (0, 0, 1, 0, "Cafeteria", "2026-08-06 12:00:00"),   # Weapon
        (0, 0, 0, 1, "Store Room", "2026-08-06 03:00:00"),  # Fire
        (1, 0, 0, 0, "Cafeteria", "2026-08-06 11:00:00"),   # Day normal - no alert
    ]
    
    for tc in test_cases:
        person, vehicle, weapon, fire, loc, dt_str = tc
        dt = datetime.strptime(dt_str, "%Y-%m-%d %H:%M:%S")
        alert, reason = should_alert(
            person_count=person,
            vehicle_count=vehicle,
            weapon_count=weapon,
            fire_count=fire,
            location=loc,
            dt=dt
        )
        print(f"{dt_str} | {loc} | P:{person} V:{vehicle} W:{weapon} F:{fire} -> {alert}: {reason}")