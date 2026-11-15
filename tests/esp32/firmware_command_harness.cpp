#include <cassert>

#include "../../esp32/firmware.ino"

void expect_pwm(int ch0, int ch1, int ch2, int ch3) {
    assert(pwm_values[LEFT_RPWM] == ch0);
    assert(pwm_values[LEFT_LPWM] == ch1);
    assert(pwm_values[RIGHT_RPWM] == ch2);
    assert(pwm_values[RIGHT_LPWM] == ch3);
}

int main() {
    assert(DRIVE_SPEED == 70);
    assert(TURN_SPEED == 65);
    setup();

    parseSerialCommand("FORWARD");
    expect_pwm(DRIVE_SPEED, 0, DRIVE_SPEED, 0);

    parseSerialCommand("BACKWARD");
    expect_pwm(0, DRIVE_SPEED, 0, DRIVE_SPEED);

    parseSerialCommand("LEFT");
    expect_pwm(0, TURN_SPEED, TURN_SPEED, 0);

    parseSerialCommand("RIGHT");
    expect_pwm(TURN_SPEED, 0, 0, TURN_SPEED);

    parseSerialCommand("PATROL");
    assert(robotMode == MODE_AUTO_PATROL);
    expect_pwm(DRIVE_SPEED, 0, DRIVE_SPEED, 0);

    parseSerialCommand("STOP");
    assert(robotMode == MODE_IDLE);
    expect_pwm(0, 0, 0, 0);

    parseSerialCommand("G,40,40");
    assert(robotMode == MODE_REMOTE);
    expect_pwm(40, 0, 40, 0);

    parseSerialCommand("G,-55,55");
    expect_pwm(0, 55, 55, 0);

    parseSerialCommand("G,56,56");
    expect_pwm(55, 0, 55, 0);

    parseSerialCommand("G,invalid,40");
    assert(robotMode == MODE_IDLE);
    expect_pwm(0, 0, 0, 0);

    mock_millis = COMMAND_TIMEOUT_MS + 1;
    loop();
    assert(robotMode == MODE_IDLE);
    expect_pwm(0, 0, 0, 0);

    return 0;
}
