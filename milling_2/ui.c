/**************************************************************
 * ui.c
 * User Interface.
 **************************************************************/

#include "terminal.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "ui.h"

/* Layout constants */
#define HELP_LEFT 1
#define HELP_TOP 1
#define HELP_WIDTH 38
#define HELP_HEIGHT 16

#define POS_LEFT 40
#define POS_TOP 1
#define POS_WIDTH 21
#define POS_HEIGHT 16

#define STATUS_LEFT 1
#define STATUS_TOP 18
#define STATUS_WIDTH 60
#define STATUS_HEIGHT 4

#define INPUT_ROW 24
#define INPUT_COL 2

/* Draw one box */
static void draw_box(int left, int top, int width, int height,
                     const char *title, unsigned short background)
{
    term_set_color(clrBlack, background);

    for (int col = 0; col < width; col++)
    {
        term_move_to(left + col, top);
        printf("-");
        term_move_to(left + col, top + height - 1);
        printf("-");
    }

    for (int row = 1; row < height - 1; row++)
    {
        term_move_to(left, top + row);
        printf("|");
        term_move_to(left + width - 1, top + row);
        printf("|");
    }

    for (int row = 1; row < height - 1; row++)
    {
        for (int col = 1; col < width - 1; col++)
        {
            term_move_to(left + col, top + row);
            printf(" ");
        }
    }

    if (title != NULL && title[0] != '\0')
    {
        int pad = (width - (int)strlen(title)) / 2;
        if (pad < 1)
        {
            pad = 1;
        }
        term_move_to(left + pad, top);
        printf("%s", title);
    }

    term_set_color(clrWhite, clrBlack);
}

void ui_draw_background(void)
{
    term_set_color(clrWhite, clrBlack);
    term_cls();

    draw_box(HELP_LEFT, HELP_TOP, HELP_WIDTH, HELP_HEIGHT, "Help", clrCyan);
    draw_box(HELP_LEFT + 1, HELP_TOP + 1, HELP_WIDTH - 2, HELP_HEIGHT - 2, "", clrBlack);

    draw_box(POS_LEFT, POS_TOP, POS_WIDTH, POS_HEIGHT, "Position", clrGreen);
    draw_box(POS_LEFT + 1, POS_TOP + 1, POS_WIDTH - 2, POS_HEIGHT - 2, "", clrBlack);

    draw_box(STATUS_LEFT, STATUS_TOP, STATUS_WIDTH, STATUS_HEIGHT, "Status", clrYellow);
    draw_box(STATUS_LEFT + 1, STATUS_TOP + 1, STATUS_WIDTH - 2, STATUS_HEIGHT - 2, "", clrBlack);

    term_move_to(1, INPUT_ROW);
    printf(">");
}

void ui_draw_help_manual(void)
{
    term_move_to(HELP_LEFT + 2, HELP_TOP + 1);  printf("a / d  = X- / X+");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 2);  printf("s / w  = Y- / Y+");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 3);  printf("q / e  = Z- / Z+");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 4);  printf("+ / -  = spindle up / down");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 5);  printf("0      = spindle off");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 6);  printf("c      = command mode");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 7);  printf("h      = show this help");
}

void ui_draw_help_command(void)
{
    term_move_to(HELP_LEFT + 2, HELP_TOP + 1);   printf("M2 or M02          = return to manual mode");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 2);   printf("M3 Snnn            = spindle on, set PWM");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 3);   printf("M5                 = spindle off");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 4);   printf("G90                = absolute positioning");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 5);   printf("G91                = relative positioning");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 6);   printf("G28.1              = set current position as home");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 7);   printf("G28                = return to home position");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 8);   printf("G0 X.. Y.. Z..     = rapid move");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 9);   printf("G1 X.. Y.. Z.. F.. = linear move");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 10);  printf("G2 X.. Y.. I.. J.. = clockwise arc");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 11);  printf("G3 X.. Y.. I.. J.. = counter-clockwise arc");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 12);  printf("G4 P..             = dwell");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 13);  printf("G20                = set units to inches");
    term_move_to(HELP_LEFT + 2, HELP_TOP + 14);  printf("G21                = set units to millimeters");
}

void ui_update_positions(int x, int y, int z, uint16_t spindle, bool manual_mode)
{
    const char *mode = manual_mode ? "manual" : "command";

    term_move_to(POS_LEFT + 2, POS_TOP + 1);
    printf("MODE: %-8s", mode);

    term_move_to(POS_LEFT + 2, POS_TOP + 3);
    printf("X: %-8d", x);

    term_move_to(POS_LEFT + 2, POS_TOP + 4);
    printf("Y: %-8d", y);

    term_move_to(POS_LEFT + 2, POS_TOP + 5);
    printf("Z: %-8d", z);

    term_move_to(POS_LEFT + 2, POS_TOP + 7);
    printf("SPD: %-8u", spindle);
}

void ui_show_status(const char *message)
{
    for (int row = 1; row < STATUS_HEIGHT - 1; row++)
    {
        term_move_to(STATUS_LEFT + 1, STATUS_TOP + row);
        printf("%-*s", STATUS_WIDTH - 2, "");
    }

    term_move_to(STATUS_LEFT + 2, STATUS_TOP + 1);
    printf("%-*s", STATUS_WIDTH - 4, message);
}

void ui_show_error(const char *message)
{
    for (int row = 1; row < STATUS_HEIGHT - 1; row++)
    {
        term_move_to(STATUS_LEFT + 1, STATUS_TOP + row);
        printf("%-*s", STATUS_WIDTH - 2, "");
    }

    term_move_to(STATUS_LEFT + 2, STATUS_TOP + 1);
    printf("ERROR: %-*s", STATUS_WIDTH - 11, message);
}

void ui_clear_input_line(void)
{
    term_move_to(1, INPUT_ROW);
    term_erase_line();
    printf(">");
}

void ui_place_input_cursor(int index)
{
    term_move_to(INPUT_COL + index, INPUT_ROW);
}