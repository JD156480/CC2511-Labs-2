/**************************************************************
 * machine.c (motion and control)
 * Shared machine state and motion/helper functions.
 *
 * Responsibilities:
 * - shared machine globals
 * - menu printing
 * - limit clamping
 * - feed-rate conversion
 * - move helpers
 * - arc execution
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "machine.h"
#include "mmhal.h"

/* ---------------- Shared machine state ---------------- */

bool manual_mode = true;      /* start in manual mode */
uint16_t spindle_pwm = 0;     /* current spindle PWM level */
bool metric_mode = true;      /* true = mm, false = inches */
bool absolute_mode = true;    /* true = absolute, false = relative */

int pos_x = 0;
int pos_y = 0;
int pos_z = 0;

int home_x = 0;
int home_y = 0;
int home_z = 0;

/* ---------------- Menu display ---------------- */

/* Show manual-mode help */
void print_manual_menu(void)
{
    printf("\n");
    printf("=== MANUAL MODE ===\n");
    printf("Current position: X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
    printf("Spindle PWM: %u\n", spindle_pwm);
    printf("Keys:\n");
    printf("  a / d  = X- / X+\n");
    printf("  s / w  = Y- / Y+\n");
    printf("  q / e  = Z- / Z+\n");
    printf("  + / -  = spindle up / down\n");
    printf("  0      = spindle off\n");
    printf("  c      = command mode\n");
    printf("  h      = show this help\n");
    printf("\n");
}

/* Show command-mode help */
void print_command_menu(void)
{
    printf("\n");
    printf("=== COMMAND MODE ===\n");
    printf("Current position: X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
    printf("Home position:    X=%d Y=%d Z=%d\n", home_x, home_y, home_z);
    printf("Positioning mode: %s\n", absolute_mode ? "ABSOLUTE" : "RELATIVE");
    printf("Spindle PWM: %u\n", spindle_pwm);
    printf("Commands:\n");
    printf("  M2 or M02           = return to manual mode\n");
    printf("  M3 Snnn             = spindle on, set PWM\n");
    printf("  M5                  = spindle off\n");
    printf("  G90                 = absolute positioning\n");
    printf("  G91                 = relative positioning\n");
    printf("  G28.1               = set current position as home\n");
    printf("  G28                 = return to home position\n");
    printf("  G0 X.. Y.. Z..      = rapid move\n");
    printf("  G1 X.. Y.. Z.. F..  = linear move\n");
    printf("  G2 X.. Y.. I.. J..  = clockwise arc\n");
    printf("  G3 X.. Y.. I.. J..  = counter-clockwise arc\n");
    printf("  G4 P..              = dwell\n");
    printf("  G20                 = set units to inches\n");
    printf("  G21                 = set units to millimeters\n");
    printf("  H or HELP           = show this help\n");
    printf("\n");
}

/* ---------------- Limit helpers ---------------- */

/* Clamp a value into a minimum/maximum range */
int clamp_value(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

/* Clamp a target position to the valid range for one axis */
int clamp_axis_target(int target, int axis)
{
    if (axis == XDIM)
    {
        return clamp_value(target, X_MIN, X_MAX);
    }
    else if (axis == YDIM)
    {
        return clamp_value(target, Y_MIN, Y_MAX);
    }
    else
    {
        return clamp_value(target, Z_MIN, Z_MAX);
    }
}

/* ---------------- Feed-rate / motion helpers ---------------- */

/* Convert G-code feed rate into step pulse timing */
void set_feed_rate(int feed_rate)
{
    if (feed_rate <= 0)
    {
        return;
    }

    /* Clamp allowed feed-rate range */
    if (feed_rate < 50)
    {
        feed_rate = 50;
    }
    if (feed_rate > 2000)
    {
        feed_rate = 2000;
    }

    /* Keep STEP high time fixed and vary the low gap */
    mmhal_high_delay_us = 5;
    mmhal_low_delay_us = 100000 / feed_rate;

    /* Prevent delay becoming too small */
    if (mmhal_low_delay_us < 50)
    {
        mmhal_low_delay_us = 50;
    }
}

/* Move one axis from current position to target position */
void move_axis_to_target(int *current_pos, int target_pos, int axis)
{
    int limited_target = clamp_axis_target(target_pos, axis);

    if (limited_target != target_pos)
    {
        printf("Axis limit reached\n");
    }

    int delta = limited_target - *current_pos;

    if (delta > 0)
    {
        for (int i = 0; i < delta; i++)
        {
            if (axis == XDIM)
            {
                mmhal_step_motors(1, 0, 0);
            }
            else if (axis == YDIM)
            {
                mmhal_step_motors(0, 1, 0);
            }
            else
            {
                mmhal_step_motors(0, 0, 1);
            }
        }
    }
    else if (delta < 0)
    {
        for (int i = 0; i < -delta; i++)
        {
            if (axis == XDIM)
            {
                mmhal_step_motors(-1, 0, 0);
            }
            else if (axis == YDIM)
            {
                mmhal_step_motors(0, -1, 0);
            }
            else
            {
                mmhal_step_motors(0, 0, -1);
            }
        }
    }

    *current_pos = limited_target;
}

/* Parse X/Y/Z values from a command string and convert to step targets */
void parse_target_position(const char *cmd_buf, int *target_x, int *target_y, int *target_z)
{
    char *x_ptr = strchr((char *)cmd_buf, 'X');
    char *y_ptr = strchr((char *)cmd_buf, 'Y');
    char *z_ptr = strchr((char *)cmd_buf, 'Z');

    /* Default to current position if axis not supplied */
    *target_x = pos_x;
    *target_y = pos_y;
    *target_z = pos_z;

    if (x_ptr != NULL)
    {
        int value = atoi(x_ptr + 1);

        if (metric_mode)
        {
            value = value * X_STEPS_PER_MM;
        }
        else
        {
            value = value * 254 / 10;      /* inches to mm */
            value = value * X_STEPS_PER_MM;
        }

        *target_x = absolute_mode ? value : pos_x + value;
    }

    if (y_ptr != NULL)
    {
        int value = atoi(y_ptr + 1);

        if (metric_mode)
        {
            value = value * Y_STEPS_PER_MM;
        }
        else
        {
            value = value * 254 / 10;
            value = value * Y_STEPS_PER_MM;
        }

        *target_y = absolute_mode ? value : pos_y + value;
    }

    if (z_ptr != NULL)
    {
        int value = atoi(z_ptr + 1);

        if (metric_mode)
        {
            value = value * Z_STEPS_PER_MM;
        }
        else
        {
            value = value * 254 / 10;
            value = value * Z_STEPS_PER_MM;
        }

        *target_z = absolute_mode ? value : pos_z + value;
    }
}

/* Execute a full XYZ move */
void execute_move(int target_x, int target_y, int target_z, const char *label)
{
    printf("%s starting\n", label);

    move_axis_to_target(&pos_x, target_x, XDIM);
    move_axis_to_target(&pos_y, target_y, YDIM);
    move_axis_to_target(&pos_z, target_z, ZDIM);

    printf("%s move complete\n", label);
    printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
}

/**
 * @brief Execute a circular arc move using integer step logic.
 *
 * @param end_x     Target X in steps
 * @param end_y     Target Y in steps
 * @param i_offset  Offset from current X to arc centre
 * @param j_offset  Offset from current Y to arc centre
 * @param clockwise true = G02, false = G03
 */
void execute_arc(int end_x, int end_y, int i_offset, int j_offset, bool clockwise)
{
    int cx = pos_x + i_offset;
    int cy = pos_y + j_offset;

    printf("ARC centre X=%d Y=%d  end X=%d Y=%d\n", cx, cy, end_x, end_y);

    int rx = pos_x - cx;
    int ry = pos_y - cy;
    int ex = end_x - cx;
    int ey = end_y - cy;

    int x = rx;
    int y = ry;

    int steps_taken = 0;
    const int MAX_ARC_STEPS = (X_MAX + Y_MAX) * 4;

    while (steps_taken < MAX_ARC_STEPS)
    {
        if (x == ex && y == ey)
        {
            break;
        }

        int next_x;
        int next_y;

        if (clockwise)
        {
            if (y > 0 && (x >= 0 || abs(x) <= abs(y)))
            {
                next_x = x + 1;
                next_y = y;
            }
            else if (x > 0 && (y <= 0 || abs(y) <= abs(x)))
            {
                next_x = x;
                next_y = y - 1;
            }
            else if (y < 0 && (x <= 0 || abs(x) <= abs(y)))
            {
                next_x = x - 1;
                next_y = y;
            }
            else
            {
                next_x = x;
                next_y = y + 1;
            }
        }
        else
        {
            if (y > 0 && (x <= 0 || abs(x) <= abs(y)))
            {
                next_x = x - 1;
                next_y = y;
            }
            else if (x < 0 && (y >= 0 || abs(y) <= abs(x)))
            {
                next_x = x;
                next_y = y + 1;
            }
            else if (y < 0 && (x >= 0 || abs(x) <= abs(y)))
            {
                next_x = x + 1;
                next_y = y;
            }
            else
            {
                next_x = x;
                next_y = y - 1;
            }
        }

        int dx = next_x - x;
        int dy = next_y - y;

        int new_abs_x = clamp_axis_target(cx + next_x, XDIM);
        int new_abs_y = clamp_axis_target(cy + next_y, YDIM);

        mmhal_step_motors(dx, dy, 0);

        pos_x = new_abs_x;
        pos_y = new_abs_y;

        x = next_x;
        y = next_y;
        steps_taken++;
    }

    /* Snap to exact end point */
    move_axis_to_target(&pos_x, end_x, XDIM);
    move_axis_to_target(&pos_y, end_y, YDIM);

    printf("ARC complete\n");
    printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
}