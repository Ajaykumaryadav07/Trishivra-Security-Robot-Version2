#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>

constexpr int OUTPUT = 1;
constexpr int INPUT = 0;
constexpr int HIGH = 1;
constexpr int LOW = 0;
constexpr int CHANGE = 2;

#define IRAM_ATTR

inline int pwm_values[64] = {};
inline unsigned long mock_millis = 0;
inline unsigned long mock_micros = 0;

class String {
public:
    String() = default;
    String(const char* value) : value_(value) {}
    String(std::string value) : value_(std::move(value)) {}

    void trim() {
        const auto first = value_.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            value_.clear();
            return;
        }
        const auto last = value_.find_last_not_of(" \t\r\n");
        value_ = value_.substr(first, last - first + 1);
    }

    size_t length() const { return value_.length(); }

    bool equalsIgnoreCase(const char* other) const {
        std::string lhs = value_;
        std::string rhs = other;
        std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lhs == rhs;
    }

    bool startsWith(const char* prefix) const {
        return value_.rfind(prefix, 0) == 0;
    }

    int indexOf(char value, size_t from = 0) const {
        const auto position = value_.find(value, from);
        return position == std::string::npos ? -1 : static_cast<int>(position);
    }

    String substring(size_t start) const {
        return String(value_.substr(start));
    }

    String substring(size_t start, size_t end) const {
        return String(value_.substr(start, end - start));
    }

    long toInt() const {
        try {
            return std::stol(value_);
        } catch (...) {
            return 0;
        }
    }

    char charAt(size_t index) const { return value_.at(index); }
    const std::string& str() const { return value_; }
    String& operator+=(char value) {
        value_ += value;
        return *this;
    }

private:
    std::string value_;
};

class MockSerial {
public:
    void begin(unsigned long) {}
    int available() const { return static_cast<int>(input_.size() - input_pos_); }
    char read() { return input_.at(input_pos_++); }
    void println(const char* value = "") { output_ += std::string(value) + "\n"; }
    void println(const String& value) { println(value.str().c_str()); }
    void println(int value) { println(std::to_string(value).c_str()); }
    void println(float value, int precision = 2) {
        std::ostringstream stream;
        stream.precision(precision);
        stream << std::fixed << value;
        println(stream.str().c_str());
    }
    void print(const char* value) { output_ += value; }
    void print(const String& value) { print(value.str().c_str()); }
    void print(char value) { output_ += value; }
    void print(int value) { output_ += std::to_string(value); }
    void print(float value, int precision = 2) {
        std::ostringstream stream;
        stream.precision(precision);
        stream << std::fixed << value;
        print(stream.str().c_str());
    }

private:
    std::string input_;
    size_t input_pos_ = 0;
    std::string output_;
};

inline MockSerial Serial;

inline bool ledcAttach(int, int, int) { return true; }
inline void ledcWrite(int channel, int value) { pwm_values[channel] = value; }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return LOW; }
inline int digitalPinToInterrupt(int pin) { return pin; }
inline void attachInterruptArg(int, void (*)(void*), void*, int) {}
inline void analogReadResolution(int) {}
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
inline unsigned long millis() { return mock_millis; }
inline unsigned long micros() { return mock_micros; }

template <typename T>
inline T constrain(T value, T low, T high) {
    return std::max(low, std::min(value, high));
}
