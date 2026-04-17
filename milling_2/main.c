/**************************************************************
 * main.c
 * High-level machine control loop.
 *************************************************************/

#include <stdio.h>
#include "pico/stdlib.h"
#include "mmhal.h"
#include "machine.h"
#include "manual.h"
#include "command.h"
#include "ui.h"

int main(void)
{
    /* Initialise stdio first */
    stdio_init_all();

    /* Give terminal a moment to attach */
    sleep_ms(1500);

    /* Initialise hardware before claiming init is OK */
    mmhal_init();

    /* Draw UI once startup is genuinely complete */
    ui_draw_background();
    ui_draw_help_manual();
    ui_update_positions(pos_x, pos_y, pos_z, spindle_pwm, manual_mode);
    ui_show_status("Init OK");
    ui_clear_input_line();
    ui_place_input_cursor(0);

    while (true)
    {
        if (manual_mode)
        {
            handle_manual_mode();
        }
        else
        {
            handle_command_mode();
        }

        sleep_ms(10);
    }
}