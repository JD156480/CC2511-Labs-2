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

#define STEP_AMOUNT 20   /* manual jog distance in steps */

/* Handle single-key controls in manual mode */
void handle_manual_mode(void)
{
    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT)
    {
        return;
    }

    switch (ch)
    {
        /* Manual X- jog */
        case 'a':
            move_axis_to_target(&pos_x, pos_x - STEP_AMOUNT, XDIM);
            printf("X-\n");
            printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
            break;

        /* Manual X+ jog */
        case 'd':
            move_axis_to_target(&pos_x, pos_x + STEP_AMOUNT, XDIM);
            printf("X+\n");
            printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
            break;

        /* Manual Y+ jog */
        case 'w':
            move_axis_to_target(&pos_y, pos_y + STEP_AMOUNT, YDIM);
            printf("Y+\n");
            printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
            break;

        /* Manual Y- jog */
        case 's':
            move_axis_to_target(&pos_y, pos_y - STEP_AMOUNT, YDIM);
            printf("Y-\n");
            printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
            break;

        /* Manual Z+ jog */
        case 'e':
            move_axis_to_target(&pos_z, pos_z + STEP_AMOUNT, ZDIM);
            printf("Z+\n");
            printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
            break;

        /* Manual Z- jog */
        case 'q':
            move_axis_to_target(&pos_z, pos_z - STEP_AMOUNT, ZDIM);
            printf("Z-\n");
            printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
            break;

        /* Increase spindle PWM */
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
            printf("Spindle PWM = %u\n", spindle_pwm);
            break;

        /* Decrease spindle PWM */
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
            printf("Spindle PWM = %u\n", spindle_pwm);
            break;

        /* Spindle off */
        case '0':
            spindle_pwm = 0;
            mmhal_set_spindle_pwm(spindle_pwm);
            printf("Spindle OFF\n");
            break;

        /* Switch to command mode */
        case 'c':
            manual_mode = false;
            printf("Switched to command mode\n");
            print_command_menu();
            break;

        /* Show manual help */
        case 'h':
            print_manual_menu();
            break;

        /* Any other key */
        default:
            printf("Unknown key: %c\n", ch);
            break;
    }
}