/**************************************************************
 * manual.c
 * Manual mode controls for jogging and spindle adjustment.
 **************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "manual.h"
#include "machine.h"
#include "mmhal.h"
#include "ui.h"

#define STEP_AMOUNT 20 /* manual jog distance in steps */

void handle_manual_mode(void)
{
    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT)
    {
        return;
    }

    switch (ch)
    {
    case 'a':
    case 'A':
        move_axis_to_target(&pos_x, pos_x - STEP_AMOUNT, XDIM);
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Moved X-");
        break;

    case 'd':
    case 'D':
        move_axis_to_target(&pos_x, pos_x + STEP_AMOUNT, XDIM);
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Moved X+");
        break;

    case 'w':
    case 'W':
        move_axis_to_target(&pos_y, pos_y + STEP_AMOUNT, YDIM);
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Moved Y+");
        break;

    case 's':
    case 'S':
        move_axis_to_target(&pos_y, pos_y - STEP_AMOUNT, YDIM);
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Moved Y-");
        break;

    case 'e':
    case 'E':
        move_axis_to_target(&pos_z, pos_z + STEP_AMOUNT, ZDIM);
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Moved Z+");
        break;

    case 'q':
    case 'Q':
        move_axis_to_target(&pos_z, pos_z - STEP_AMOUNT, ZDIM);
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Moved Z-");
        break;

    case '+':
        if (spindle_pwm <= 245)
        {
            spindle_pwm += 10;
        }
        else
        {
            spindle_pwm = 255;
        }
        mmhal_set_spindle_pwm(spindle_pwm);
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Spindle PWM updated");
        break;

    case '-':
        if (spindle_pwm >= 10)
        {
            spindle_pwm -= 10;
        }
        else
        {
            spindle_pwm = 0;
        }
        mmhal_set_spindle_pwm(spindle_pwm);
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Spindle PWM updated");
        break;

    case '0':
        spindle_pwm = 0;
        mmhal_set_spindle_pwm(spindle_pwm);
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Spindle OFF");
        break;

    case 'c':
    case 'C':
        manual_mode = false;
        ui_draw_background();
        ui_draw_help_command();
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Switched to command mode");
        ui_clear_input_line();
        ui_place_input_cursor(0);
        break;

    case 'h':
    case 'H':
        print_manual_menu();
        ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
        ui_show_status("Manual mode");
        break;

    default:
        ui_show_status("Unknown key");
        break;
    }
}