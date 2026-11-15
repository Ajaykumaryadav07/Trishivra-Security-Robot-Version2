cmake_minimum_required(VERSION 3.10)
project(security_robot_v2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== OPTIMIZATION FLAGS =====
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Enable optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Enable Link Time Optimization (LTO) for faster code
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
if(IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Additional performance flags
if(UNIX AND NOT APPLE)
    add_compile_options(-fno-strict-aliasing -ffast-math)
    add_compile_options(-flto)  # Link-time optimization
endif()

# Find OpenCV
find_package(OpenCV REQUIRED)

# Include directories
include_directories(include)
include_directories(${OpenCV_INCLUDE_DIRS})

# Define a platform-aware temporary directory at compile time
if(WIN32)
    set(DEFAULT_TMP "$ENV{TEMP}")
else()
    set(DEFAULT_TMP "/tmp")
endif()

# Expose PROJECT_TMP_DIR as a string macro to C++ sources
add_definitions(-DPROJECT_TMP_DIR=\"${DEFAULT_TMP}\")

# Source files
set(SOURCES
    src/main.cpp
    src/camera.cpp
    src/yolo_detector.cpp
    src/motor_controller.cpp
    src/navigation.cpp
    src/state_machine.cpp
    src/depth_estimator.cpp
    src/slam.cpp
    src/steering_controller.cpp
    src/odometry.cpp
    src/imu_mpu6050.cpp
    src/gps_receiver.cpp
    src/voice_controller.cpp
    src/utils.cpp
)

# Executable
add_executable(security_robot ${SOURCES})

# Optimization settings for executable
set_target_properties(security_robot PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION ${IPO_SUPPORTED}
)

if(WIN32)
    target_link_libraries(security_robot ${OpenCV_LIBS})
else()
    # Link with -s for strip symbols (smaller binary)
    target_link_libraries(security_robot ${OpenCV_LIBS} pthread)
    set_target_properties(security_robot PROPERTIES LINK_FLAGS "-s")
endif()

# Install
install(TARGETS security_robot DESTINATION bin)