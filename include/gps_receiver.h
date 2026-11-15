#ifndef GPS_RECEIVER_H
#define GPS_RECEIVER_H

#include <chrono>
#include <string>

struct GPSFix {
    double latitude = 0.0;
    double longitude = 0.0;
    double speed_mps = 0.0;
    double course_deg = 0.0;
    bool has_course = false;
    std::chrono::steady_clock::time_point received_at{};
};

class GPSReceiver {
public:
    GPSReceiver() = default;
    ~GPSReceiver();

    bool open(const std::string& port = "/dev/serial0", int baud = 9600);
    void close();
    bool poll(GPSFix& fix);
    bool is_open() const;

    static bool parse_sentence(const std::string& sentence, GPSFix& fix);

private:
    int fd = -1;
    std::string buffer;
    GPSFix latest_fix;
    bool has_fix = false;
    double course_anchor_latitude = 0.0;
    double course_anchor_longitude = 0.0;
    bool has_course_anchor = false;
};

#endif
