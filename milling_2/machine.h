#ifndef __MACHINE_H__
#define __MACHINE_H__

#include <stdbool.h>
#include <stdint.h>
#include "mmhal.h"

/* Steps per mm for each axis */
#define X_STEPS_PER_MM 80
#define Y_STEPS_PER_MM 80
#define Z_STEPS_PER_MM 80

/* Travel limits in mm */
#define X_MIN_MM 0
#define X_MAX_MM 200
#define Y_MIN_MM 0
#define Y_MAX_MM 200
#define Z_MIN_MM -100
#define Z_MAX_MM 200

/* Travel limits converted to steps */
#define X_MIN (X_MIN_MM * X_STEPS_PER_MM)
#define X_MAX (X_MAX_MM * X_STEPS_PER_MM)
#define Y_MIN (Y_MIN_MM * Y_STEPS_PER_MM)
#define Y_MAX (Y_MAX_MM * Y_STEPS_PER_MM)
#define Z_MIN (Z_MIN_MM * Z_STEPS_PER_MM)
#define Z_MAX (Z_MAX_MM * Z_STEPS_PER_MM)

/* ---------------- Shared machine state ---------------- */

/* true = manual mode, false = command mode */
extern bool manual_mode;

/* spindle PWM 0-255 */
extern uint16_t spindle_pwm;

/* true = mm, false = inches */
extern bool metric_mode;

/* true = absolute (G90), false = relative (G91) */
extern bool absolute_mode;

/* current machine position in steps */
extern int pos_x;
extern int pos_y;
extern int pos_z;

/* stored home position in steps */
extern int home_x;
extern int home_y;
extern int home_z;

/* ---------------- Menu display ---------------- */
void print_manual_menu(void);
void print_command_menu(void);

/* ---------------- Helper functions ---------------- */
int clamp_value(int value, int min_value, int max_value);
int clamp_axis_target(int target, int axis);
void set_feed_rate(int feed_rate);
void move_axis_to_target(int *current_pos, int target_pos, int axis);
void parse_target_position(const char *cmd_buf, int *target_x, int *target_y, int *target_z);
void execute_move(int target_x, int target_y, int target_z, const char *label);
void execute_arc(int end_x, int end_y, int i_offset, int j_offset, bool clockwise);

#endif // __MACHINE_H__