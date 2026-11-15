#include "robot_modules.h"

#include <cassert>
#include <cmath>
#include <string>
#include <vector>

int main() {
    std::vector<Pose> waypoints;
    std::string mode;
    bool autonomous_movement = true;
    bool require_gps = false;
    double initial_heading_deg = -1.0;
    double origin_latitude = 0.0;
    double origin_longitude = 0.0;

    const bool enabled = load_navigation_waypoints(
        "config/navigation_waypoints.yaml", waypoints, mode,
        autonomous_movement, require_gps, initial_heading_deg,
        origin_latitude, origin_longitude);

    assert(!enabled);
    assert(mode == "simulation");
    assert(!autonomous_movement);
    assert(require_gps);
    assert(initial_heading_deg == 0.0);
    assert(waypoints.size() == 5);
    assert(std::abs(origin_latitude - 26.886653) < 1e-8);
    assert(std::abs(origin_longitude - 81.056797) < 1e-8);
    assert(std::abs(waypoints[1].y - 50.1f) < 0.5f);
    assert(std::abs(waypoints[2].x + 44.7f) < 0.5f);
    assert(std::abs(waypoints[3].y + 50.1f) < 0.5f);
    assert(std::abs(waypoints[4].x) < 0.5f);
    return 0;
}
