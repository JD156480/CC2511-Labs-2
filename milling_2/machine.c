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
#include "ui.h"

/* ---------------- Shared machine state ---------------- */

bool manual_mode = true;   /* start in manual mode */
uint16_t spindle_pwm = 0;  /* current spindle PWM level */
bool metric_mode = true;   /* true = mm, false = inches */
bool absolute_mode = true; /* true = absolute, false = relative */

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
    ui_draw_background();
    ui_draw_help_manual();
    ui_show_status("Manual mode");
    ui_clear_input_line();
    ui_place_input_cursor(0);
}

/* Show command-mode help */
void print_command_menu(void)
{
    ui_draw_background();
    ui_draw_help_command();
    ui_show_status("Command mode");
    ui_clear_input_line();
    ui_place_input_cursor(0);
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
        ui_show_error("Axis limit reached");
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
            value = value * 254 / 10; /* inches to mm */
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
    char msg[40];

    snprintf(msg, sizeof(msg), "%s starting", label);
    ui_show_status(msg);

    move_axis_to_target(&pos_x, target_x, XDIM);
    move_axis_to_target(&pos_y, target_y, YDIM);
    move_axis_to_target(&pos_z, target_z, ZDIM);

    ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
    snprintf(msg, sizeof(msg), "%s move complete", label);
    ui_show_status(msg);
}

/**
 * @brief Execute a circular arc move.
 * Temporary safe fallback version.
 */
void execute_arc(int end_x, int end_y, int i_offset, int j_offset, bool clockwise)
{
    char msg[48];

    (void)i_offset;
    (void)j_offset;

    snprintf(msg, sizeof(msg), clockwise ? "G2 fallback move" : "G3 fallback move");
    ui_show_status(msg);

    move_axis_to_target(&pos_x, end_x, XDIM);
    move_axis_to_target(&pos_y, end_y, YDIM);

    ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
    ui_show_status(clockwise ? "CW arc complete" : "CCW arc complete");
}