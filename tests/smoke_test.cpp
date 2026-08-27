#include <iostream>
#include "robot_modules.h"

int main() {
    if (camera_status().empty() || yolo_status().empty()) {
        std::cerr << "Smoke test failed" << std::endl;
        return 1;
    }
    std::cout << "Smoke test passed" << std::endl;
    return 0;
}
