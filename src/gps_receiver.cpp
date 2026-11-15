#include "gps_receiver.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {
constexpr double pi = 3.14159265358979323846;

bool parse_coordinate(const std::string& value, const std::string& hemisphere,
                      int degree_digits, double maximum, double& coordinate) {
    if (value.size() <= static_cast<size_t>(degree_digits) ||
        (hemisphere != "N" && hemisphere != "S" &&
         hemisphere != "E" && hemisphere != "W")) {
        return false;
    }

    char* end = nullptr;
    const double raw = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0' || !std::isfinite(raw) || raw < 0.0) {
        return false;
    }

    const double degrees = std::floor(raw / 100.0);
    const double minutes = raw - degrees * 100.0;
    if (minutes < 0.0 || minutes >= 60.0 || degrees > maximum) {
        return false;
    }

    coordinate = degrees + minutes / 60.0;
    if (coordinate > maximum) {
        return false;
    }
    if (hemisphere == "S" || hemisphere == "W") {
        coordinate = -coordinate;
    }
    return true;
}

bool parse_number(const std::string& value, double& result) {
    if (value.empty()) {
        return false;
    }
    char* end = nullptr;
    result = std::strtod(value.c_str(), &end);
    return end != value.c_str() && *end == '\0' && std::isfinite(result);
}

bool split_nmea(const std::string& input, std::vector<std::string>& fields) {
    std::string sentence = input;
    while (!sentence.empty() &&
           (sentence.back() == '\r' || sentence.back() == '\n')) {
        sentence.pop_back();
    }
    if (sentence.size() < 7 || sentence.front() != '$') {
        return false;
    }

    const size_t checksum_pos = sentence.rfind('*');
    if (checksum_pos == std::string::npos || checksum_pos + 3 != sentence.size()) {
        return false;
    }
    char* end = nullptr;
    const long expected = std::strtol(sentence.c_str() + checksum_pos + 1, &end, 16);
    if (end != sentence.c_str() + sentence.size() || expected < 0 || expected > 255) {
        return false;
    }

    unsigned char actual = 0;
    for (size_t i = 1; i < checksum_pos; ++i) {
        actual ^= static_cast<unsigned char>(sentence[i]);
    }
    if (actual != expected) {
        return false;
    }

    std::stringstream stream(sentence.substr(1, checksum_pos - 1));
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return !fields.empty();
}

double distance_m(double lat1, double lon1, double lat2, double lon2) {
    constexpr double earth_radius_m = 6371000.0;
    const double lat1_rad = lat1 * pi / 180.0;
    const double lat2_rad = lat2 * pi / 180.0;
    const double delta_lat = (lat2 - lat1) * pi / 180.0;
    const double delta_lon = (lon2 - lon1) * pi / 180.0;
    const double a = std::sin(delta_lat / 2.0) * std::sin(delta_lat / 2.0) +
        std::cos(lat1_rad) * std::cos(lat2_rad) *
        std::sin(delta_lon / 2.0) * std::sin(delta_lon / 2.0);
    return earth_radius_m * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

double bearing_deg(double lat1, double lon1, double lat2, double lon2) {
    const double lat1_rad = lat1 * pi / 180.0;
    const double lat2_rad = lat2 * pi / 180.0;
    const double delta_lon = (lon2 - lon1) * pi / 180.0;
    const double y = std::sin(delta_lon) * std::cos(lat2_rad);
    const double x = std::cos(lat1_rad) * std::sin(lat2_rad) -
        std::sin(lat1_rad) * std::cos(lat2_rad) * std::cos(delta_lon);
    double bearing = std::atan2(y, x) * 180.0 / pi;
    return std::fmod(bearing + 360.0, 360.0);
}

}  // namespace

GPSReceiver::~GPSReceiver() {
    close();
}

bool GPSReceiver::open(const std::string& port, int baud) {
#ifdef _WIN32
    (void)port;
    (void)baud;
    std::cerr << "[GPS] UART receiver is supported on Linux only" << std::endl;
    return false;
#else
    close();
    fd = ::open(port.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "[GPS] Failed to open " << port << ": "
                  << std::strerror(errno) << std::endl;
        return false;
    }

    termios settings{};
    if (tcgetattr(fd, &settings) != 0) {
        std::cerr << "[GPS] Failed to read UART settings: "
                  << std::strerror(errno) << std::endl;
        close();
        return false;
    }
    speed_t speed;
    switch (baud) {
        case 4800: speed = B4800; break;
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        default:
            std::cerr << "[GPS] Unsupported UART baud rate: " << baud << std::endl;
            close();
            return false;
    }
    cfsetispeed(&settings, speed);
    cfsetospeed(&settings, speed);
    settings.c_cflag = (settings.c_cflag & ~(CSIZE | PARENB | CSTOPB | CRTSCTS)) |
        CS8 | CLOCAL | CREAD;
    settings.c_iflag = 0;
    settings.c_lflag = 0;
    settings.c_oflag = 0;
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &settings) != 0) {
        std::cerr << "[GPS] Failed to configure UART: "
                  << std::strerror(errno) << std::endl;
        close();
        return false;
    }
    buffer.clear();
    has_fix = false;
    has_course_anchor = false;
    return true;
#endif
}

void GPSReceiver::close() {
#ifndef _WIN32
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
#endif
}

bool GPSReceiver::is_open() const {
    return fd >= 0;
}

bool GPSReceiver::parse_sentence(const std::string& sentence, GPSFix& fix) {
    std::vector<std::string> fields;
    if (!split_nmea(sentence, fields) || fields[0].size() < 3) {
        return false;
    }

    const std::string type = fields[0].substr(fields[0].size() - 3);
    GPSFix parsed = fix;
    if (type == "GGA") {
        double quality = 0.0;
        if (fields.size() <= 6 || !parse_number(fields[6], quality)) {
            return false;
        }
        if (quality <= 0.0) {
            return false;
        }
        if (fields.size() <= 5 ||
            !parse_coordinate(fields[2], fields[3], 2, 90.0, parsed.latitude) ||
            !parse_coordinate(fields[4], fields[5], 3, 180.0, parsed.longitude)) {
            return false;
        }
    } else if (type == "RMC") {
        if (fields.size() <= 6 || fields[2] != "A" ||
            !parse_coordinate(fields[3], fields[4], 2, 90.0, parsed.latitude) ||
            !parse_coordinate(fields[5], fields[6], 3, 180.0, parsed.longitude)) {
            return false;
        }
        double speed_knots = 0.0;
        double course = 0.0;
        if ((fields.size() > 7 && !fields[7].empty() &&
             !parse_number(fields[7], speed_knots)) ||
            (fields.size() > 8 && !fields[8].empty() &&
             !parse_number(fields[8], course)) ||
            speed_knots < 0.0 || course < 0.0 || course >= 360.0) {
            return false;
        }
        parsed.speed_mps = speed_knots * 0.514444;
        parsed.course_deg = course;
        parsed.has_course = fields.size() > 8 && !fields[8].empty() &&
            parsed.speed_mps >= 0.5;
    } else {
        return false;
    }
    parsed.received_at = std::chrono::steady_clock::now();
    fix = parsed;
    return true;
}

bool GPSReceiver::poll(GPSFix& fix) {
#ifdef _WIN32
    (void)fix;
    return false;
#else
    if (fd < 0) {
        return false;
    }

    char chunk[512];
    bool updated = false;
    for (;;) {
        const ssize_t count = ::read(fd, chunk, sizeof(chunk));
        if (count > 0) {
            buffer.append(chunk, static_cast<size_t>(count));
            if (buffer.size() > 4096) {
                buffer.erase(0, buffer.size() - 4096);
            }
        } else if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
                   errno != EINTR) {
            std::cerr << "[GPS] UART read failed: " << std::strerror(errno)
                      << std::endl;
            return false;
        } else {
            break;
        }
    }

    size_t newline;
    while ((newline = buffer.find('\n')) != std::string::npos) {
        const std::string line = buffer.substr(0, newline);
        buffer.erase(0, newline + 1);
        GPSFix sentence_fix = latest_fix;
        if (!parse_sentence(line, sentence_fix)) {
            continue;
        }
        latest_fix = sentence_fix;
        has_fix = true;
        updated = true;

        if (latest_fix.has_course && latest_fix.speed_mps >= 0.5) {
            course_anchor_latitude = latest_fix.latitude;
            course_anchor_longitude = latest_fix.longitude;
            has_course_anchor = true;
        } else {
            latest_fix.has_course = false;
            if (!has_course_anchor) {
                course_anchor_latitude = latest_fix.latitude;
                course_anchor_longitude = latest_fix.longitude;
                has_course_anchor = true;
            } else if (distance_m(course_anchor_latitude, course_anchor_longitude,
                                  latest_fix.latitude, latest_fix.longitude) >= 3.0) {
                latest_fix.course_deg = bearing_deg(
                    course_anchor_latitude, course_anchor_longitude,
                    latest_fix.latitude, latest_fix.longitude);
                latest_fix.has_course = true;
                course_anchor_latitude = latest_fix.latitude;
                course_anchor_longitude = latest_fix.longitude;
            }
        }
    }

    if (has_fix) {
        fix = latest_fix;
    }
    return updated;
#endif
}
