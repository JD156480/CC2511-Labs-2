/**************************************************************
 * command.c
 * Command mode / G-code input handling.
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
#include "ui.h"

#define CMD_BUF_SIZE 64

static int linear_value_to_steps(float value, int steps_per_mm)
{
    float mm_value;

    if (metric_mode)
    {
        mm_value = value;
    }
    else
    {
        mm_value = value * 25.4f;
    }

    return (int)(mm_value * (float)steps_per_mm);
}

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

void handle_command_mode(void)
{
    static char cmd_buf[CMD_BUF_SIZE];
    static int cmd_index = 0;

    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT)
    {
        return;
    }

    if (ch == '\r' || ch == '\n')
    {
        parsed_command_t parsed;

        if (cmd_index == 0)
        {
            return;
        }

        cmd_buf[cmd_index] = '\0';

        if (!parse_command(cmd_buf, &parsed))
        {
            char msg[64];
            snprintf(msg, sizeof(msg), "Unknown command %s", cmd_buf);
            ui_show_error(msg);
            cmd_index = 0;
            ui_clear_input_line();
            ui_place_input_cursor(cmd_index);
            return;
        }

        switch (parsed.type)
        {
        case CMD_M2:
            manual_mode = true;
            print_manual_menu();
            ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
            ui_show_status("Switched to manual mode");
            break;

        case CMD_M3:
            if (parsed.has_s)
            {
                int value = (int)parsed.s;

                if (value < 0)
                    value = 0;
                if (value > 150)
                    value = 150;

                spindle_pwm = (uint16_t)value;
                mmhal_set_spindle_pwm(spindle_pwm);

                ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
                ui_show_status("Spindle ON");
            }
            else
            {
                ui_show_error("M3 requires S value");
            }
            break;

        case CMD_M5:
            spindle_pwm = 0;
            mmhal_set_spindle_pwm(0);
            ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
            ui_show_status("Spindle OFF");
            break;

        case CMD_G90:
            absolute_mode = true;
            ui_show_status("Positioning mode ABSOLUTE");
            break;

        case CMD_G91:
            absolute_mode = false;
            ui_show_status("Positioning mode RELATIVE");
            break;

        case CMD_G281:
            home_x = pos_x;
            home_y = pos_y;
            home_z = pos_z;
            ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
            ui_show_status("Home position set");
            break;

        case CMD_G28:
            execute_move(home_x, home_y, home_z, "G28");
            ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
            ui_show_status("Returned home");
            break;

        case CMD_G4:
            if (parsed.has_p)
            {
                int dwell_ms = (int)parsed.p;

                if (dwell_ms < 0)
                {
                    dwell_ms = 0;
                }

                ui_show_status("Dwelling...");
                sleep_ms(dwell_ms);
                ui_show_status("Dwell complete");
            }
            else
            {
                ui_show_error("G4 requires P value");
            }
            break;

        case CMD_G20:
            metric_mode = false;
            ui_show_status("Units INCHES");
            break;

        case CMD_G21:
            metric_mode = true;
            ui_show_status("Units MILLIMETERS");
            break;

        case CMD_G0:
        {
            int target_x;
            int target_y;
            int target_z;

            build_target_positions(&parsed, &target_x, &target_y, &target_z);
            execute_move(target_x, target_y, target_z, "G0");
            ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
            ui_show_status("Rapid move complete");
            break;
        }

        case CMD_G1:
        {
            int target_x;
            int target_y;
            int target_z;

            build_target_positions(&parsed, &target_x, &target_y, &target_z);

            if (parsed.has_f)
            {
                set_feed_rate((int)parsed.f);
            }

            execute_move(target_x, target_y, target_z, "G1");
            ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
            ui_show_status("Linear move complete");
            break;
        }

        case CMD_G2:
        {
            int target_x;
            int target_y;
            int target_z;

            build_target_positions(&parsed, &target_x, &target_y, &target_z);
            (void)target_z;

            if (!parsed.has_i || !parsed.has_j)
            {
                ui_show_error("G2 requires I and J");
            }
            else
            {
                int i_steps = linear_value_to_steps(parsed.i, X_STEPS_PER_MM);
                int j_steps = linear_value_to_steps(parsed.j, Y_STEPS_PER_MM);
                execute_arc(target_x, target_y, i_steps, j_steps, true);
                ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
                ui_show_status("CW arc complete");
            }
            break;
        }

        case CMD_G3:
        {
            int target_x;
            int target_y;
            int target_z;

            build_target_positions(&parsed, &target_x, &target_y, &target_z);
            (void)target_z;

            if (!parsed.has_i || !parsed.has_j)
            {
                ui_show_error("G3 requires I and J");
            }
            else
            {
                int i_steps = linear_value_to_steps(parsed.i, X_STEPS_PER_MM);
                int j_steps = linear_value_to_steps(parsed.j, Y_STEPS_PER_MM);
                execute_arc(target_x, target_y, i_steps, j_steps, false);
                ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
                ui_show_status("CCW arc complete");
            }
            break;
        }

        case CMD_HELP:
            print_command_menu();
            ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
            break;

        default:
            ui_show_error("Command not implemented");
            break;
        }

        cmd_index = 0;
        ui_clear_input_line();
        ui_place_input_cursor(cmd_index);
        return;
    }
    else if (ch == '\b' || ch == 127)
    {
        if (cmd_index > 0)
        {
            cmd_index--;
            ui_clear_input_line();
            for (int i = 0; i < cmd_index; i++)
            {
                putchar(cmd_buf[i]);
            }
            ui_place_input_cursor(cmd_index);
        }
    }
    else if (isprint((unsigned char)ch) && cmd_index < CMD_BUF_SIZE - 1)
    {
        ch = toupper((unsigned char)ch);
        cmd_buf[cmd_index] = (char)ch;
        cmd_index++;
        putchar(ch);
        ui_place_input_cursor(cmd_index);
    }
    else if (cmd_index >= CMD_BUF_SIZE - 1)
    {
        cmd_index = 0;
        ui_clear_input_line();
        ui_place_input_cursor(0);
        ui_show_error("Command too long");
    }
}