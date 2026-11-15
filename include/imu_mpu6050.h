#ifndef IMU_MPU6050_H
#define IMU_MPU6050_H

#include <chrono>
#include <cstdint>
#include <string>

class MPU6050 {
public:
    MPU6050() = default;
    ~MPU6050();

    bool initialize(const std::string& device, std::uint8_t address,
                    double initial_heading_deg);
    bool update();
    double heading_rad() const { return heading_rad_; }

private:
    bool write_register(std::uint8_t reg, std::uint8_t value);
    bool read_gyro_z(std::int16_t& value);
    void close_device();

    int fd_ = -1;
    std::uint8_t address_ = 0x68;
    double gyro_z_bias_ = 0.0;
    double heading_rad_ = 0.0;
    std::chrono::steady_clock::time_point last_update_{};
};

#endif
