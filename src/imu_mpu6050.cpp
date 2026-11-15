#include "imu_mpu6050.h"

#include <cmath>
#include <iostream>
#include <thread>

#ifdef __linux__
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double gyro_lsb_per_degree_per_second = 131.0;
constexpr std::uint8_t gyro_zout_high_register = 0x47;
constexpr int calibration_samples = 300;

double normalize_angle(double angle) {
    return std::remainder(angle, 2.0 * pi);
}
}

MPU6050::~MPU6050() {
    close_device();
}

bool MPU6050::initialize(const std::string& device, std::uint8_t address,
                         double initial_heading_deg) {
#ifdef __linux__
    close_device();
    if (!std::isfinite(initial_heading_deg) ||
        initial_heading_deg < 0.0 || initial_heading_deg >= 360.0) {
        std::cerr << "[IMU] Invalid initial heading" << std::endl;
        return false;
    }

    fd_ = ::open(device.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "[IMU] Failed to open " << device << ": "
                  << std::strerror(errno) << std::endl;
        return false;
    }
    address_ = address;
    if (ioctl(fd_, I2C_SLAVE, address_) < 0 ||
        !write_register(0x6B, 0x00) ||
        !write_register(0x1B, 0x00) ||
        !write_register(0x1A, 0x03)) {
        std::cerr << "[IMU] Failed to initialize MPU-6050 at I2C address 0x"
                  << std::hex << static_cast<int>(address_) << std::dec
                  << std::endl;
        close_device();
        return false;
    }

    double bias_sum = 0.0;
    for (int sample = 0; sample < calibration_samples; ++sample) {
        std::int16_t raw_z = 0;
        if (!read_gyro_z(raw_z)) {
            std::cerr << "[IMU] Gyroscope read failed during calibration"
                      << std::endl;
            close_device();
            return false;
        }
        bias_sum += raw_z;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    gyro_z_bias_ = bias_sum / calibration_samples;

    const double heading_bearing_rad = initial_heading_deg * pi / 180.0;
    heading_rad_ = normalize_angle(pi / 2.0 - heading_bearing_rad);
    last_update_ = std::chrono::steady_clock::now();
    std::cout << "[IMU] MPU-6050 ready; gyro bias calibrated, initial heading "
              << initial_heading_deg << " deg" << std::endl;
    return true;
#else
    (void)device;
    (void)address;
    (void)initial_heading_deg;
    std::cerr << "[IMU] MPU-6050 I2C support is available on Linux only"
              << std::endl;
    return false;
#endif
}

bool MPU6050::update() {
#ifdef __linux__
    if (fd_ < 0) {
        return false;
    }

    std::int16_t raw_z = 0;
    if (!read_gyro_z(raw_z)) {
        std::cerr << "[IMU] Gyroscope read failed" << std::endl;
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - last_update_).count();
    last_update_ = now;
    if (!std::isfinite(dt) || dt <= 0.0 || dt > 2.0) {
        std::cerr << "[IMU] Heading update interval is stale" << std::endl;
        return false;
    }

    const double rate_rad_per_second =
        ((raw_z - gyro_z_bias_) / gyro_lsb_per_degree_per_second) * pi / 180.0;
    heading_rad_ = normalize_angle(heading_rad_ + rate_rad_per_second * dt);
    return true;
#else
    return false;
#endif
}

bool MPU6050::write_register(std::uint8_t reg, std::uint8_t value) {
#ifdef __linux__
    const std::uint8_t data[] = {reg, value};
    return ::write(fd_, data, sizeof(data)) == static_cast<ssize_t>(sizeof(data));
#else
    (void)reg;
    (void)value;
    return false;
#endif
}

bool MPU6050::read_gyro_z(std::int16_t& value) {
#ifdef __linux__
    std::uint8_t reg = gyro_zout_high_register;
    std::uint8_t data[2] = {};
    i2c_msg messages[2] = {};
    messages[0].addr = address_;
    messages[0].len = 1;
    messages[0].buf = &reg;
    messages[1].addr = address_;
    messages[1].flags = I2C_M_RD;
    messages[1].len = sizeof(data);
    messages[1].buf = data;
    i2c_rdwr_ioctl_data transfer{};
    transfer.msgs = messages;
    transfer.nmsgs = 2;
    if (ioctl(fd_, I2C_RDWR, &transfer) < 0) {
        return false;
    }
    value = static_cast<std::int16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8) | data[1]);
    return true;
#else
    (void)value;
    return false;
#endif
}

void MPU6050::close_device() {
#ifdef __linux__
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}
