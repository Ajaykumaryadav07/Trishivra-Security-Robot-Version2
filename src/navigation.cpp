#include "robot_modules.h"
#include "robot_state.h"
#include "config.h"
#include <cmath>
#include <queue>
#include <vector>
#include <algorithm>
#include <iostream>
#include <limits>

// ============================================================
// NavigationModule Implementation
// ============================================================

NavigationModule::NavigationModule() 
    : current_waypoint_index(0), 
      current_pose{0.0f, 0.0f, 0.0f},
      goal{0.0f, 0.0f, 0.0f} {
    std::cout << "[NAV] Navigation module initialized" << std::endl;
}

NavigationModule::~NavigationModule() {
    std::cout << "[NAV] Navigation module destroyed" << std::endl;
}

// ============================================================
// Waypoint Management
// ============================================================

void NavigationModule::set_waypoints(const std::vector<Pose>& waypoints) {
    this->waypoints = waypoints;
    current_waypoint_index = 0;
    path.clear();
    
    if (!waypoints.empty()) {
        goal = waypoints[0];
        std::cout << "[NAV] Waypoints set: " << waypoints.size() << " waypoints" << std::endl;
    } else {
        std::cout << "[NAV] No waypoints provided" << std::endl;
    }
}

void NavigationModule::set_goal(const Pose& goal) {
    this->goal = goal;
    path.clear();
    std::cout << "[NAV] Goal set: (" << goal.x << ", " << goal.y << ")" << std::endl;
}

void NavigationModule::update_pose(const Pose& pose) {
    current_pose = pose;
    
    // Update global state
    g_robot_state.x = pose.x;
    g_robot_state.y = pose.y;
    g_robot_state.theta = pose.theta;
}

// ============================================================
// Path Planning (A* Algorithm)
// ============================================================

std::vector<Pose> NavigationModule::plan_path() {
    // Return existing path if still valid
    if (!path.empty()) {
        return path;
    }
    
    if (is_goal_reached()) {
        path.clear();
        return path;
    }
    
    if (!map.empty()) {
        path = a_star(current_pose, goal);
    }
    
    if (path.size() < 2) {
        if (!map.empty()) {
            std::cout << "[NAV] No path found on map, falling back to direct path" << std::endl;
        }
        path.clear();
        path.push_back(current_pose);
        path.push_back(goal);
    } else {
        std::cout << "[NAV] Path found with " << path.size() << " waypoints" << std::endl;
    }
    
    return path;
}

std::vector<Pose> NavigationModule::a_star(const Pose& start, const Pose& goal) {
    std::vector<Pose> result_path;
    
    // If start and goal are the same
    float dx = start.x - goal.x;
    float dy = start.y - goal.y;
    if (std::sqrt(dx*dx + dy*dy) < WAYPOINT_TOLERANCE) {
        result_path.push_back(start);
        return result_path;
    }
    
    // ============================================================
    // Simplified A* Implementation
    // ============================================================
    
    // Grid resolution for path planning
    const float grid_resolution = 0.1f;  // 10cm per grid cell
    const int grid_width = static_cast<int>(MAP_WIDTH / grid_resolution);
    const int grid_height = static_cast<int>(MAP_HEIGHT / grid_resolution);
    const float half_width = MAP_WIDTH * 0.5f;
    const float half_height = MAP_HEIGHT * 0.5f;
    
    // Convert world coordinates to grid coordinates
    auto world_to_grid = [&](float wx, float wy) -> std::pair<int, int> {
        int gx = static_cast<int>((wx + half_width) / grid_resolution);
        int gy = static_cast<int>((wy + half_height) / grid_resolution);
        return {gx, gy};
    };
    
    // Convert grid coordinates to world coordinates
    auto grid_to_world = [&](int gx, int gy) -> std::pair<float, float> {
        float wx = gx * grid_resolution - half_width;
        float wy = gy * grid_resolution - half_height;
        return {wx, wy};
    };
    
    // Grid nodes
    struct GridNode {
        int x, y;
        float g_cost;  // Cost from start
        float h_cost;  // Heuristic to goal
        float f_cost;  // Total cost
        int parent_x, parent_y;
        bool closed;
        bool open;
        
        GridNode() : x(0), y(0), g_cost(std::numeric_limits<float>::max()),
                     h_cost(0), f_cost(std::numeric_limits<float>::max()),
                     parent_x(-1), parent_y(-1), closed(false), open(false) {}
    };
    
    // Create grid
    std::vector<std::vector<GridNode>> grid(grid_height, std::vector<GridNode>(grid_width));
    
    // Initialize grid nodes
    for (int y = 0; y < grid_height; y++) {
        for (int x = 0; x < grid_width; x++) {
            grid[y][x].x = x;
            grid[y][x].y = y;
        }
    }
    
    // Get start and goal grid positions
    auto [start_x, start_y] = world_to_grid(start.x, start.y);
    auto [goal_x, goal_y] = world_to_grid(goal.x, goal.y);
    
    // Clamp to grid bounds
    start_x = std::max(0, std::min(grid_width - 1, start_x));
    start_y = std::max(0, std::min(grid_height - 1, start_y));
    goal_x = std::max(0, std::min(grid_width - 1, goal_x));
    goal_y = std::max(0, std::min(grid_height - 1, goal_y));
    
    // If goal is out of bounds
    if (goal_x < 0 || goal_x >= grid_width || goal_y < 0 || goal_y >= grid_height) {
        result_path.push_back(start);
        result_path.push_back(goal);
        return result_path;
    }
    
    // Initialize start node
    grid[start_y][start_x].g_cost = 0.0f;
    grid[start_y][start_x].h_cost = std::abs(start_x - goal_x) + std::abs(start_y - goal_y);
    grid[start_y][start_x].f_cost = grid[start_y][start_x].h_cost;
    grid[start_y][start_x].open = true;
    
    // Priority queue for open nodes
    auto cmp = [&](const GridNode* a, const GridNode* b) {
        return a->f_cost > b->f_cost;
    };
    std::priority_queue<GridNode*, std::vector<GridNode*>, decltype(cmp)> open_queue(cmp);
    open_queue.push(&grid[start_y][start_x]);
    
    // A* main loop
    bool found = false;
    const int directions[8][2] = {
        {0, -1}, {1, -1}, {1, 0}, {1, 1},
        {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
    };
    
    while (!open_queue.empty()) {
        GridNode* current = open_queue.top();
        open_queue.pop();
        
        if (current->closed) continue;
        current->closed = true;
        
        // Check if goal reached
        if (current->x == goal_x && current->y == goal_y) {
            found = true;
            break;
        }
        
        // Explore neighbors
        for (const auto& dir : directions) {
            int nx = current->x + dir[0];
            int ny = current->y + dir[1];
            
            // Check bounds
            if (nx < 0 || nx >= grid_width || ny < 0 || ny >= grid_height) continue;
            
            GridNode* neighbor = &grid[ny][nx];
            if (neighbor->closed) continue;
            
            // Check if obstacle (using map data)
            bool obstacle = false;
            if (!map.empty()) {
                int map_x = static_cast<int>((nx * grid_resolution) / MAP_RESOLUTION);
                int map_y = static_cast<int>((ny * grid_resolution) / MAP_RESOLUTION);
                if (map_x >= 0 && map_x < map.cols && map_y >= 0 && map_y < map.rows) {
                    if (map.at<uchar>(map_y, map_x) < 50) {  // Occupied cell
                        obstacle = true;
                    }
                }
            }
            
            // Skip obstacles (with some tolerance for diagonal)
            if (obstacle) continue;
            
            // Calculate new cost
            float move_cost = (dir[0] != 0 && dir[1] != 0) ? 1.414f : 1.0f;  // Diagonal cost
            float new_g_cost = current->g_cost + move_cost;
            
            if (new_g_cost < neighbor->g_cost) {
                neighbor->g_cost = new_g_cost;
                neighbor->h_cost = std::abs(nx - goal_x) + std::abs(ny - goal_y);
                neighbor->f_cost = neighbor->g_cost + neighbor->h_cost;
                neighbor->parent_x = current->x;
                neighbor->parent_y = current->y;
                neighbor->open = true;
                open_queue.push(neighbor);
            }
        }
    }
    
    // Reconstruct path
    if (found) {
        int cx = goal_x, cy = goal_y;
        std::vector<std::pair<int, int>> grid_path;
        
        while (cx != start_x || cy != start_y) {
            grid_path.push_back({cx, cy});
            int px = grid[cy][cx].parent_x;
            int py = grid[cy][cx].parent_y;
            if (px < 0 || py < 0) break;
            cx = px;
            cy = py;
        }
        grid_path.push_back({start_x, start_y});
        
        // Reverse path (start to goal)
        std::reverse(grid_path.begin(), grid_path.end());
        
        // Convert grid to world coordinates
        for (const auto& gp : grid_path) {
            auto [wx, wy] = grid_to_world(gp.first, gp.second);
            Pose p;
            p.x = wx;
            p.y = wy;
            p.theta = 0.0f;
            result_path.push_back(p);
        }
    } else {
        // No path found, return direct path
        std::cout << "[NAV] No path found, using direct path" << std::endl;
        result_path.push_back(start);
        result_path.push_back(goal);
    }
    
    // Smooth path (optional)
    if (result_path.size() > 3) {
        // Simple smoothing: remove intermediate points that are in a straight line
        std::vector<Pose> smoothed;
        smoothed.push_back(result_path[0]);
        
        for (size_t i = 1; i < result_path.size() - 1; i++) {
            float angle1 = std::atan2(result_path[i].y - result_path[i-1].y,
                                      result_path[i].x - result_path[i-1].x);
            float angle2 = std::atan2(result_path[i+1].y - result_path[i].y,
                                      result_path[i+1].x - result_path[i].x);
            float angle_diff = std::abs(normalize_angle(angle2 - angle1));
            
            if (angle_diff > 0.1f) {
                smoothed.push_back(result_path[i]);
            }
        }
        smoothed.push_back(result_path.back());
        result_path = smoothed;
    }
    
    return result_path;
}

// ============================================================
// Velocity Calculation (Pure Pursuit)
// ============================================================

VelocityCmd NavigationModule::get_velocity() {
    VelocityCmd cmd{0.0f, 0.0f};
    
    // Force a fresh plan when needed
    if (path.empty()) {
        plan_path();
    }
    
    if (path.empty()) {
        return cmd;
    }
    
    if (is_goal_reached()) {
        if (!waypoints.empty()) {
            current_waypoint_index = (current_waypoint_index + 1) % waypoints.size();
            set_goal(waypoints[current_waypoint_index]);
            path.clear();
            std::cout << "[NAV] Advancing to waypoint " << current_waypoint_index << std::endl;
        }
        return cmd;
    }
    
    // Prune path points that are behind or very close
    while (path.size() > 1) {
        float dx = path[1].x - current_pose.x;
        float dy = path[1].y - current_pose.y;
        if (std::sqrt(dx*dx + dy*dy) < 0.15f) {
            path.erase(path.begin());
        } else {
            break;
        }
    }
    
    // Choose lookahead point
    float lookahead_distance = 0.5f;
    Pose target = path.back();
    for (const auto& p : path) {
        float dx = p.x - current_pose.x;
        float dy = p.y - current_pose.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist > lookahead_distance) {
            target = p;
            break;
        }
    }
    
    float dx = target.x - current_pose.x;
    float dy = target.y - current_pose.y;
    float distance = std::sqrt(dx*dx + dy*dy);
    
    if (distance < 0.05f) {
        return cmd;
    }
    
    float target_angle = std::atan2(dy, dx);
    float angle_diff = normalize_angle(target_angle - current_pose.theta);
    
    // Linear speed: reduce on sharp turns and close distances
    cmd.linear = std::min(MAX_SPEED, distance * 0.5f);
    float angle_abs = std::abs(angle_diff);
    if (angle_abs > 0.5f) {
        cmd.linear *= std::max(0.1f, 1.0f - (angle_abs - 0.5f));
    }
    
    cmd.angular = std::min(MAX_TURN_SPEED, std::max(-MAX_TURN_SPEED, angle_diff * 1.5f));
    
    if (g_robot_state.obstacle_detected) {
        cmd.linear = 0.0f;
        cmd.angular = 0.5f;
        if (g_robot_state.is_avoiding) {
            std::cout << "[NAV] Obstacle detected - holding and waiting for avoidance" << std::endl;
        }
    }
    
    return cmd;
}

// ============================================================
// Helper Functions
// ============================================================

void NavigationModule::clear_path() {
    path.clear();
}

void NavigationModule::set_map(const cv::Mat& map) {
    this->map = map.clone();
    clear_path();
    std::cout << "[NAV] Map received: " << map.cols << "x" << map.rows << std::endl;
}

Pose NavigationModule::get_current_pose() const {
    return current_pose;
}

bool NavigationModule::is_goal_reached() const {
    float dx = goal.x - current_pose.x;
    float dy = goal.y - current_pose.y;
    return std::sqrt(dx*dx + dy*dy) < WAYPOINT_TOLERANCE;
}

float NavigationModule::normalize_angle(float angle) {
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
    return angle;
}

// ============================================================
    
    // Define ROI in front of robot (center region)
    int start_x = std::max(0, center_x - roi_size);
    int end_x = std::min(depth_map.cols, center_x + roi_size);
    int start_y = std::max(0, center_y - 10);
    int end_y = std::min(depth_map.rows, center_y + 40);  // Forward direction
    
    float min_depth = 100.0f;
    
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            float depth = depth_map.at<float>(y, x);
            if (depth > 0.1f && depth < min_depth) {
                min_depth = depth;
            }
        }
    }
    
    return min_depth < min_distance;
}
*/

// ============================================================
// Dynamic Replanning
// ============================================================

/*
void NavigationModule::replan_if_needed() {
    static int replan_count = 0;
    replan_count++;
    
    // Replan every 10 frames or if goal changed
    if (replan_count % 10 == 0 || path.empty()) {
        path = plan_path();
    }
    
    // If path is empty or too short, force replan
    if (path.size() < 2) {
        path = plan_path();
    }
}
*/

// ============================================================
// Path Visualization (Optional)
// ============================================================

/*
cv::Mat NavigationModule::visualize_path(const cv::Mat& background) {
    cv::Mat vis;
    if (background.empty()) {
        vis = cv::Mat::zeros(480, 640, CV_8UC3);
    } else {
        vis = background.clone();
    }
    
    // Draw path
    for (size_t i = 1; i < path.size(); i++) {
        cv::Point p1(static_cast<int>(path[i-1].x * 100 + 320),
                     static_cast<int>(path[i-1].y * 100 + 240));
        cv::Point p2(static_cast<int>(path[i].x * 100 + 320),
                     static_cast<int>(path[i].y * 100 + 240));
        cv::line(vis, p1, p2, cv::Scalar(0, 255, 0), 2);
    }
    
    // Draw goal
    cv::Point goal_p(static_cast<int>(goal.x * 100 + 320),
                     static_cast<int>(goal.y * 100 + 240));
    cv::circle(vis, goal_p, 10, cv::Scalar(0, 0, 255), -1);
    
    return vis;
}
*/