#ifndef __UI_H__
#define __UI_H__

#include <stdbool.h>
#include <stdint.h>

void ui_draw_background(void);
void ui_draw_help_manual(void);
void ui_draw_help_command(void);
void ui_update_positions(int x, int y, int z, uint16_t spindle, bool manual_mode);
void ui_show_status(const char *message);
void ui_show_error(const char *message);
void ui_clear_input_line(void);
void ui_place_input_cursor(int index);

#endif