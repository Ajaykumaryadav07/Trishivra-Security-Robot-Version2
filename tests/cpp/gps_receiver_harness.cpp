#include "gps_receiver.h"

#include <cassert>
#include <cmath>
#include <string>

namespace {

std::string with_checksum(const std::string& body) {
    unsigned char checksum = 0;
    for (unsigned char byte : body) {
        checksum ^= byte;
    }
    constexpr char digits[] = "0123456789ABCDEF";
    std::string sentence = "$" + body + "*";
    sentence += digits[checksum >> 4];
    sentence += digits[checksum & 0x0F];
    return sentence;
}

}  // namespace

int main() {
    GPSFix fix;
    const std::string gga = with_checksum(
        "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    assert(GPSReceiver::parse_sentence(gga, fix));
    assert(std::abs(fix.latitude - 48.1173) < 1e-6);
    assert(std::abs(fix.longitude - 11.5166666667) < 1e-6);

    const std::string rmc = with_checksum(
        "GPRMC,123519,A,4807.038,N,01131.000,E,2.0,90.0,230394,,,A");
    assert(GPSReceiver::parse_sentence(rmc, fix));
    assert(std::abs(fix.speed_mps - 1.028888) < 1e-5);
    assert(fix.has_course);
    assert(std::abs(fix.course_deg - 90.0) < 1e-9);

    const std::string stationary_rmc = with_checksum(
        "GPRMC,123520,A,4807.038,N,01131.000,E,,,230394,,,A");
    assert(GPSReceiver::parse_sentence(stationary_rmc, fix));
    assert(!fix.has_course);

    std::string invalid = gga;
    invalid.back() = invalid.back() == '0' ? '1' : '0';
    assert(!GPSReceiver::parse_sentence(invalid, fix));

    const std::string no_fix = with_checksum(
        "GPGGA,123519,,,,,0,00,99.99,,,,,,");
    assert(!GPSReceiver::parse_sentence(no_fix, fix));
    return 0;
}
