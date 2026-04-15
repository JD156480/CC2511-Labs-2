/**************************************************************
 * command.c
 * Command mode / G-code input handling.
 *
 * Responsibilities:
 * - read one full line from Putty
 * - parse it into a structured command
 * - execute the command using machine/mmhal helpers
 **************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "mmhal.h"
#include "machine.h"
#include "command.h"
#include "parser.h"

#define CMD_BUF_SIZE 64

/* -----------------------------------------------------------
 * Convert one linear value into steps.
 * Uses current unit mode:
 * - mm  -> steps directly
 * - in  -> inches to mm, then mm to steps
 * ----------------------------------------------------------- */
static int linear_value_to_steps(float value, int steps_per_mm)
{
    int whole = (int)value;

    if (metric_mode)
    {
        return whole * steps_per_mm;
    }
    else
    {
        /* inches to mm = *25.4 -> use 254/10 integer version */
        return (whole * 254 / 10) * steps_per_mm;
    }
}

/* -----------------------------------------------------------
 * Build target X/Y/Z positions from parsed command.
 * If an axis is missing, keep current position.
 * If mode is relative, add offset to current position.
 * ----------------------------------------------------------- */
static void build_target_positions(const parsed_command_t *cmd,
                                   int *target_x,
                                   int *target_y,
                                   int *target_z)
{
    *target_x = pos_x;
    *target_y = pos_y;
    *target_z = pos_z;

    if (cmd->has_x)
    {
        int value = linear_value_to_steps(cmd->x, X_STEPS_PER_MM);
        *target_x = absolute_mode ? value : pos_x + value;
    }

    if (cmd->has_y)
    {
        int value = linear_value_to_steps(cmd->y, Y_STEPS_PER_MM);
        *target_y = absolute_mode ? value : pos_y + value;
    }

    if (cmd->has_z)
    {
        int value = linear_value_to_steps(cmd->z, Z_STEPS_PER_MM);
        *target_z = absolute_mode ? value : pos_z + value;
    }
}

/* -----------------------------------------------------------
 * Read one full text command, parse it, and execute it.
 * ----------------------------------------------------------- */
void handle_command_mode(void)
{
    static char cmd_buf[CMD_BUF_SIZE];
    static int cmd_index = 0;

    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT)
    {
        return;
    }

    /* Enter = line complete */
    if (ch == '\r' || ch == '\n')
    {
        parsed_command_t parsed;

        printf("\n");

        /* Ignore empty Enter */
        if (cmd_index == 0)
        {
            return;
        }

        /* Turn buffered chars into a C string */
        cmd_buf[cmd_index] = '\0';
        printf("CMD: %s\n", cmd_buf);

        /* Parse line into structured command */
        if (!parse_command(cmd_buf, &parsed))
        {
            printf("Unknown command: %s\n", cmd_buf);
            cmd_index = 0;
            return;
        }

        /* Execute based on parsed command type */
        switch (parsed.type)
        {
            /* Return to manual mode */
            case CMD_M2:
                manual_mode = true;
                printf("Switched to manual mode\n");
                print_manual_menu();
                break;

            /* Spindle on */
            case CMD_M3:
                if (parsed.has_s)
                {
                    int value = (int)parsed.s;

                    if (value < 0) value = 0;
                    if (value > 255) value = 255;

                    spindle_pwm = (uint16_t)value;
                    mmhal_set_spindle_pwm(spindle_pwm);
                    printf("Spindle ON, PWM = %u\n", spindle_pwm);
                }
                else
                {
                    printf("Error: M3 requires S value\n");
                }
                break;

            /* Spindle off */
            case CMD_M5:
                spindle_pwm = 0;
                mmhal_set_spindle_pwm(0);
                printf("Spindle OFF\n");
                break;

            /* Absolute mode */
            case CMD_G90:
                absolute_mode = true;
                printf("Positioning mode = ABSOLUTE\n");
                break;

            /* Relative mode */
            case CMD_G91:
                absolute_mode = false;
                printf("Positioning mode = RELATIVE\n");
                break;

            /* Set home position */
            case CMD_G281:
                home_x = pos_x;
                home_y = pos_y;
                home_z = pos_z;
                printf("Home position set to X%d Y%d Z%d\n", home_x, home_y, home_z);
                break;

            /* Return to home */
            case CMD_G28:
                execute_move(home_x, home_y, home_z, "G28");
                break;

            /* Dwell */
            case CMD_G4:
                if (parsed.has_p)
                {
                    int dwell_ms = (int)parsed.p;

                    if (dwell_ms < 0)
                    {
                        dwell_ms = 0;
                    }

                    printf("Dwelling for %d ms\n", dwell_ms);
                    sleep_ms(dwell_ms);
                    printf("Dwell complete\n");
                }
                else
                {
                    printf("Error: G4 requires P value\n");
                }
                break;

            /* Set inches */
            case CMD_G20:
                metric_mode = false;
                printf("Units = INCHES\n");
                break;

            /* Set millimeters */
            case CMD_G21:
                metric_mode = true;
                printf("Units = MILLIMETERS\n");
                break;

            /* Rapid move */
            case CMD_G0:
            {
                int target_x;
                int target_y;
                int target_z;

                build_target_positions(&parsed, &target_x, &target_y, &target_z);
                execute_move(target_x, target_y, target_z, "G0");
                break;
            }

            /* Linear move */
            case CMD_G1:
            {
                int target_x;
                int target_y;
                int target_z;

                build_target_positions(&parsed, &target_x, &target_y, &target_z);

                if (parsed.has_f)
                {
                    set_feed_rate((int)parsed.f);
                    printf("Feed rate = %d\n", (int)parsed.f);
                }

                execute_move(target_x, target_y, target_z, "G1");
                break;
            }

            /* Clockwise arc */
            case CMD_G2:
            {
                int target_x;
                int target_y;
                int target_z;

                build_target_positions(&parsed, &target_x, &target_y, &target_z);

                if (!parsed.has_i || !parsed.has_j)
                {
                    printf("Error: G2 requires I and J parameters\n");
                }
                else
                {
                    int i_steps = linear_value_to_steps(parsed.i, X_STEPS_PER_MM);
                    int j_steps = linear_value_to_steps(parsed.j, Y_STEPS_PER_MM);
                    execute_arc(target_x, target_y, i_steps, j_steps, true);
                }
                break;
            }

            /* Counter-clockwise arc */
            case CMD_G3:
            {
                int target_x;
                int target_y;
                int target_z;

                build_target_positions(&parsed, &target_x, &target_y, &target_z);

                if (!parsed.has_i || !parsed.has_j)
                {
                    printf("Error: G3 requires I and J parameters\n");
                }
                else
                {
                    int i_steps = linear_value_to_steps(parsed.i, X_STEPS_PER_MM);
                    int j_steps = linear_value_to_steps(parsed.j, Y_STEPS_PER_MM);
                    execute_arc(target_x, target_y, i_steps, j_steps, false);
                }
                break;
            }

            /* Help */
            case CMD_HELP:
                print_command_menu();
                break;

            default:
                printf("Command recognised but not fully implemented yet\n");
                break;
        }

        /* Reset input buffer for next command */
        cmd_index = 0;
        return;
    }

    /* Backspace / delete */
    else if (ch == '\b' || ch == 127)
    {
        if (cmd_index > 0)
        {
            cmd_index--;
            printf("\b \b");
        }
    }

    /* Normal input character */
    else if (cmd_index < CMD_BUF_SIZE - 1)
    {
        /* Store uppercase so commands are case-insensitive */
        cmd_buf[cmd_index] = (char)toupper(ch);
        cmd_index++;
        putchar(ch);
    }

    /* Buffer overflow protection */
    else
    {
        cmd_index = 0;
        printf("Error: command too long\n");
    }
}