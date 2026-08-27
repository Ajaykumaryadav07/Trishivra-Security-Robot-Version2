# Security Robot V2

This project is a robotics security platform with both Python-based monitoring and C++ modules for embedded-style control. It is designed to run on a Raspberry Pi or a desktop development machine, with support for camera monitoring, alerting, voice interaction, and optional AI-based detection workflows.

## Project Overview

The repository includes:

- Python runtime modules for monitoring, state handling, Telegram alerts, and voice interaction
- C++ modules for navigation, motor control, camera handling, SLAM, and state logic
- Configuration files for robot behavior and patrol routes
- A basic test scaffold for smoke testing

## Recommended Environment

### Hardware

- Raspberry Pi 4/5 or a modern Linux desktop
- Raspberry Pi Camera or USB camera
- Optional: GPIO-connected motors, sensors, or an ESP32 bridge

### Software

- Python 3.10 or newer
- pip and virtualenv
- CMake and a C++17 compiler
- OpenCV and supporting system libraries

## Quick Start

### 1. Clone or copy the project

```bash
git clone <your-repo-url>
cd security_robot_v2
```

### 2. Create a virtual environment

```bash
python3 -m venv venv
source venv/bin/activate
```

### 3. Install system dependencies

On Raspberry Pi OS / Ubuntu-based systems:

```bash
sudo apt update
sudo apt install -y build-essential cmake git python3-pip python3-venv \
    libopencv-dev libgl1 libglib2.0-0 libsm6 libxext6 libxrender1 libgomp1 \
    portaudio19-dev ffmpeg
```

If you plan to use the Pi camera:

```bash
sudo apt install -y python3-picamera2
```

### 4. Install Python dependencies

```bash
pip install --upgrade pip
pip install -r requirements.txt
```

### 5. Build the C++ binary

```bash
make
```

If you prefer CMake directly:

```bash
cmake -S . -B build
cmake --build build
```

## Run the Project

### Python entry point

```bash
python3 run_robot.py
```

### C++ binary

```bash
./build/security_robot_v2
```

## Configuration

The main settings are stored under the config folder:

- config/robot_settings.yaml: robot behavior and system settings
- config/waypoints.yaml: patrol and navigation waypoints

Update these files before deployment to match your robot hardware and environment.

## Raspberry Pi Deployment Notes

For deployment on a Raspberry Pi:

1. Enable the camera interface using `sudo raspi-config`.
2. Ensure required serial/GPIO interfaces are available if your hardware uses them.
3. Run the application from a terminal or configure it as a system service for unattended operation.
4. If using Telegram alerts, make sure your bot token and chat ID are configured securely.

## Optional AI and Vision Features

Some features may require extra packages such as:

- OpenVINO
- Ultralytics
- Picamera2

These are already listed in the requirements file and can be installed as part of the standard setup.

## Troubleshooting

- If OpenCV import fails, reinstall the system libraries and Python package.
- If camera access fails, verify the camera is enabled and the device permissions are correct.
- If the build fails, confirm that CMake and a C++17-capable compiler are installed.
- If voice features fail, install `portaudio19-dev` and the Python voice packages.

## Project Structure

- src/: C++ modules for camera, YOLO, depth, SLAM, navigation, motor control, and voice logic
- include/: shared headers for the C++ implementation
- python/: Python monitoring, alerting, and control logic
- config/: runtime configuration files
- esp32/: firmware placeholder for ESP32 communication
- tests/: smoke tests and basic validation
