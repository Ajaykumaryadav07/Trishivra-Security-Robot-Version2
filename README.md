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

- Raspberry Pi 4/5 for camera runtime; a modern Linux desktop can be used for development
- Raspberry Pi Camera Module (CSI)
- Optional: GPIO-connected motors, sensors, or an ESP32 bridge

### Software

- Python 3.10 or newer
- pip and virtualenv
- CMake and a C++17 compiler
- OpenCV for image processing and supporting system libraries

The camera runtime uses the Raspberry Pi CSI camera only; it does not fall back
to a USB webcam or laptop camera. OpenCV remains required for frame processing,
detection, and image saving.

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

The Python entry point provides camera monitoring and alerts; it does not
directly drive the motors. The C++ binary owns GPS waypoint navigation and
requires the motor controller, GPS receiver, and enabled route configuration.

### C++ binary

```bash
./build/security_robot
```

## Configuration

The main settings are stored under the config folder:

- config/robot_settings.yaml: robot behavior and system settings
- config/waypoints.yaml: action/duration patrol metadata for the Python guard
- config/navigation_waypoints.yaml: GPS coordinates and autonomous-drive safety gates

Autonomous navigation uses the NEO-6M for position and the MPU-6050 gyro for
short-term heading feedback. The trail remains disabled by default until the
hardware is tested and its coordinates and starting heading are confirmed. The
existing ESP32 ultrasonic sensors remain the obstacle safety layer.

The NEO-6M has no reliable heading while stationary. The MPU-6050 gyro supplies
short-term relative heading for steering, but has no compass and will drift over
time. Set `initial_heading_deg` to the robot's actual starting compass bearing
and align the robot to that direction before starting; heading hold is only as
good as this alignment and the gyro calibration. It stops on stale/lost GPS or
IMU data and after the final waypoint. This experimental GPS/IMU navigation is
not suitable for roads, people, or other uncontrolled environments. Test only
in a clear, private area with an operator at an accessible physical emergency
stop.

### MPU-6050 on Raspberry Pi

Connect the MPU-6050 to the Pi's I2C bus: VCC/VIN to 3.3V (physical pin 1),
GND to pin 6, SDA to pin 3, and SCL to pin 5. Enable I2C with `sudo raspi-config`
and verify the module appears at address `0x68` using `i2cdetect -y 1`. The Pi
runtime reads `/dev/i2c-1` directly and calibrates the stationary Z-axis gyro
at startup; keep the robot still during calibration. Mount the board level with
its Z axis upward. The MPU-6050 cannot independently find north, so configure
the known starting direction in `config/navigation_waypoints.yaml`.

The ESP32 firmware uses low PWM values (`70/255` forward, `65/255` turn) and
accepts bounded differential commands from the Pi, capped at `55/255`. Actual
motor speed and startup behavior depend on the driver, battery, and load; test
with the wheels raised first.

Update these files before deployment to match your robot hardware and environment.

### Telegram configuration

Keep the bot token out of tracked configuration. Export the credentials in the
shell or service that starts the robot:

```bash
export TELEGRAM_BOT_TOKEN='<token from BotFather>'
export TELEGRAM_CHAT_ID='1099781876'
python3 run_robot.py
```

The Telegram modules read these environment variables first. If a token has
ever been committed or shared, revoke it with BotFather and create a replacement.

## Raspberry Pi Deployment Notes

For deployment on a Raspberry Pi:

1. Enable the camera interface using `sudo raspi-config`.
2. Ensure required serial/GPIO interfaces are available if your hardware uses them.
3. Run the application from a terminal or configure it as a system service for unattended operation.
4. If using Telegram alerts, make sure your bot token and chat ID are configured securely.

### GPS UART and autonomous route

For a NEO-6M connected to the Pi's hardware UART, connect GPS TX to GPIO15/RXD
(physical pin 10), GPS RX to GPIO14/TXD (physical pin 8, optional), and ground
to a Pi ground pin. Use the module's supported supply voltage, but ensure its TX
signal is 3.3V-compatible; Raspberry Pi GPIO is not 5V-tolerant. Enable the
hardware UART and disable the serial login console with `sudo raspi-config`.

Run the diagnostic from the project root:

```bash
python3 scripts/check_gps.py
```

The diagnostic listens on `/dev/serial0` at 9600 baud for 15 seconds and reports
whether it received valid NMEA data and a position fix. Autonomous navigation
also reads that UART directly. Do not run `scripts/check_gps.py` at the same
time as the robot binary, because both programs would compete for the GPS
serial port. A module can transmit valid data without a satellite fix; test
outdoors and allow time for its first fix.

The supplied trail is disabled by default. After stationary and wheels-raised
checks, enable it by setting `enabled: 1`, `autonomous_movement: 1`, and
`mode: "hardware"` in `config/navigation_waypoints.yaml`. Position the robot
within 4 m of the first waypoint and point it north. Have an operator at the
physical stop throughout the low-speed test. The drive PWM is capped at 55/255,
but actual speed depends on the motors and load; GPS cannot prevent collisions
or guarantee that a waypoint is reached precisely.

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
