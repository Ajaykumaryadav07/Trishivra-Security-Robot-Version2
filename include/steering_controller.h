#ifndef STEERING_CONTROLLER_H
#define STEERING_CONTROLLER_H

#include "robot_modules.h"

// ============================================================
// Steering Controller API
// ============================================================

/**
 * Initialize the steering controller
 * @param serial Pointer to serial port object
 * @return true on success
 */
bool steering_init(void* serial);

/**
 * Set steering angle (0-180 degrees)
 * @param angle Target angle
 */
void steering_set_angle(int angle);

/**
 * Set steering angle in degrees
 * @param degrees Target angle in degrees
 */
void steering_set_angle_degrees(float degrees);

/**
 * Get current steering angle
 * @return Current angle (0-180)
 */
int steering_get_angle();

/**
 * Update steering PID controller
 * @param pot_value Current potentiometer reading
 */
void steering_update(float pot_value);

/**
 * Reset PID controller
 */
void steering_reset_pid();

/**
 * Set PID parameters
 * @param p Proportional gain
 * @param i Integral gain
 * @param d Derivative gain
 */
void steering_set_pid(float p, float i, float d);

/**
 * Calibrate potentiometer range
 * @param min_val Minimum potentiometer value
 * @param max_val Maximum potentiometer value
 */
void steering_calibrate_pot(int min_val, int max_val);

/**
 * Print steering status to console
 */
void steering_print_status();

/**
 * Check if steering is initialized
 */
bool steering_is_initialized();

#endif // STEERING_CONTROLLER_H