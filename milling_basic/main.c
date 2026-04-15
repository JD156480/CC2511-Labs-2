/**************************************************************
 * main.c 08/04/2026 CNS02 Adam, Faye
 * rev 1.0 13-Dec-2025 Bruce
 * milling_basic
 *
 * High-level machine control:
 * - manual mode key controls
 * - command mode / G-code parsing
 * - position, home and unit tracking
 * - calls mmhal.c for low-level hardware actions
 * Analogy for memory: This is the ships pilot. We're 
 * the captain telling the pilot what to do, and they
 * use mmhal.h to control mmhal.c to do the thing. 
 * 
 *************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "mmhal.h"

#define CMD_BUF_SIZE 64
#define STEP_AMOUNT 20   // manual jog distance in steps

///////////////////////////////////////////////////////////////////////
// Machine limits and step conversion
///////////////////////////////////////////////////////////////////////

// Steps per mm for each axis
#define X_STEPS_PER_MM 80
#define Y_STEPS_PER_MM 80
#define Z_STEPS_PER_MM 80

// Travel limits in mm
#define X_MIN_MM 0
#define X_MAX_MM 200
#define Y_MIN_MM 0
#define Y_MAX_MM 200
#define Z_MIN_MM 0
#define Z_MAX_MM 100

// Travel limits converted to steps
#define X_MIN (X_MIN_MM * X_STEPS_PER_MM)
#define X_MAX (X_MAX_MM * X_STEPS_PER_MM)
#define Y_MIN (Y_MIN_MM * Y_STEPS_PER_MM)
#define Y_MAX (Y_MAX_MM * Y_STEPS_PER_MM)
#define Z_MIN (Z_MIN_MM * Z_STEPS_PER_MM)
#define Z_MAX (Z_MAX_MM * Z_STEPS_PER_MM)

///////////////////////////////////////////////////////////////////////
// Global machine state
///////////////////////////////////////////////////////////////////////

bool manual_mode = true;    // start in manual mode
uint16_t spindle_pwm = 0;   // current spindle PWM level
bool metric_mode = true;    // true = mm, false = inches

// Current machine position in steps
int pos_x = 0;
int pos_y = 0;
int pos_z = 0;

// Positioning mode: true = absolute (G90), false = relative (G91)
bool absolute_mode = true;

// Stored home position in steps
int home_x = 0;
int home_y = 0;
int home_z = 0;

///////////////////////////////////////////////////////////////////////
// Menu display functions
///////////////////////////////////////////////////////////////////////

// Show manual-mode help
void print_manual_menu()
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

// Show command-mode help
void print_command_menu()
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

///////////////////////////////////////////////////////////////////////
// Limit helper functions
///////////////////////////////////////////////////////////////////////

// Clamp a value into a minimum/maximum range
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

// Clamp a target position to the valid range for one axis
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

///////////////////////////////////////////////////////////////////////
// Feed-rate and motion helper functions
///////////////////////////////////////////////////////////////////////

// Convert a G-code feed rate into step pulse timing
void set_feed_rate(int feed_rate)
{
  if (feed_rate <= 0)
  {
    return;
  }

  // Clamp allowed feed-rate range
  if (feed_rate < 50)
  {
    feed_rate = 50;
  }
  if (feed_rate > 2000)
  {
    feed_rate = 2000;
  }

  // Keep STEP high time fixed and only vary the gap between pulses
  mmhal_high_delay_us = 5;
  mmhal_low_delay_us = 100000 / feed_rate;

  // Prevent delay becoming too small
  if (mmhal_low_delay_us < 50)
  {
    mmhal_low_delay_us = 50;
  }
}

// Helper for visible test jogging - moves by several repeated steps
void jog_axis(int x_dir, int y_dir, int z_dir, int steps)
{
  for (int i = 0; i < steps; i++)
  {
    mmhal_step_motors(x_dir, y_dir, z_dir);
  }
}

// Move one axis from its current position to a target position
void move_axis_to_target(int *current_pos, int target_pos, int axis)
{
  int limited_target = clamp_axis_target(target_pos, axis);

  // Report if requested target had to be clamped
  if (limited_target != target_pos)
  {
    printf("Axis limit reached\n");
  }

  int delta = limited_target - *current_pos;

  // Move in positive direction
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
  // Move in negative direction
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

  // Save final axis position
  *current_pos = limited_target;
}

// Parse X, Y and Z values from a command string and convert to step targets
void parse_target_position(char *cmd_buf, int *target_x, int *target_y, int *target_z)
{
  char *x_ptr = strchr(cmd_buf, 'X');
  char *y_ptr = strchr(cmd_buf, 'Y');
  char *z_ptr = strchr(cmd_buf, 'Z');

  // Default to current position if an axis is not supplied
  *target_x = pos_x;
  *target_y = pos_y;
  *target_z = pos_z;

  // Parse X target
  if (x_ptr != NULL)
  {
    int value = atoi(x_ptr + 1);

    if (metric_mode)
    {
      value = value * X_STEPS_PER_MM;
    }
    else
    {
      // inches to mm, then mm to steps
      value = value * 254 / 10;
      value = value * X_STEPS_PER_MM;
    }

    *target_x = absolute_mode ? value : pos_x + value;
  }

  // Parse Y target
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

  // Parse Z target
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

// Execute a full XYZ move and report progress
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
 *
 * I and J are treated as relative to the current position.
 */
// Adam
void execute_arc(int end_x, int end_y, int i_offset, int j_offset, bool clockwise)
{
  // Centre of the arc in absolute step coordinates
  int cx = pos_x + i_offset;
  int cy = pos_y + j_offset;

  printf("ARC centre X=%d Y=%d  end X=%d Y=%d\n", cx, cy, end_x, end_y);

  // Work in coordinates relative to the arc centre
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
    // Stop when endpoint is reached
    if (x == ex && y == ey)
    {
      break;
    }

    int next_x;
    int next_y;

    // Choose next point around the circle
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

    // Convert arc step into machine X/Y motor directions
    int dx = next_x - x;
    int dy = next_y - y;

    // Clamp new absolute machine position
    int new_abs_x = clamp_axis_target(cx + next_x, XDIM);
    int new_abs_y = clamp_axis_target(cy + next_y, YDIM);

    mmhal_step_motors(dx, dy, 0);

    pos_x = new_abs_x;
    pos_y = new_abs_y;

    x = next_x;
    y = next_y;
    steps_taken++;
  }

  // Snap to exact endpoint at the end
  move_axis_to_target(&pos_x, end_x, XDIM);
  move_axis_to_target(&pos_y, end_y, YDIM);

  printf("ARC complete\n");
  printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
}

///////////////////////////////////////////////////////////////////////
// Manual mode
///////////////////////////////////////////////////////////////////////

// Handle single-key controls in manual mode
void handle_manual_mode()
{
  int ch = getchar_timeout_us(0);
  if (ch == PICO_ERROR_TIMEOUT)
  {
    return;
  }

  switch (ch)
  {
    // Manual X- jog
    case 'a':
      move_axis_to_target(&pos_x, pos_x - STEP_AMOUNT, XDIM);
      printf("X-\n");
      printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
      break;

    // Manual X+ jog
    case 'd':
      move_axis_to_target(&pos_x, pos_x + STEP_AMOUNT, XDIM);
      printf("X+\n");
      printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
      break;

    // Manual Y+ jog
    case 'w':
      move_axis_to_target(&pos_y, pos_y + STEP_AMOUNT, YDIM);
      printf("Y+\n");
      printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
      break;

    // Manual Y- jog
    case 's':
      move_axis_to_target(&pos_y, pos_y - STEP_AMOUNT, YDIM);
      printf("Y-\n");
      printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
      break;

    // Manual Z+ jog
    case 'e':
      move_axis_to_target(&pos_z, pos_z + STEP_AMOUNT, ZDIM);
      printf("Z+\n");
      printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
      break;

    // Manual Z- jog
    case 'q':
      move_axis_to_target(&pos_z, pos_z - STEP_AMOUNT, ZDIM);
      printf("Z-\n");
      printf("POS X=%d Y=%d Z=%d\n", pos_x, pos_y, pos_z);
      break;

    // Increase spindle PWM
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

    // Decrease spindle PWM
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

    // Spindle off
    case '0':
      spindle_pwm = 0;
      mmhal_set_spindle_pwm(spindle_pwm);
      printf("Spindle OFF\n");
      break;

    // Switch to command mode
    case 'c':
      manual_mode = false;
      printf("Switched to command mode\n");
      print_command_menu();
      break;

    // Show manual help
    case 'h':
      print_manual_menu();
      break;

    // Any other key
    default:
      printf("Unknown key: %c\n", ch);
      break;
  }
}

///////////////////////////////////////////////////////////////////////
// Command mode / G-code handling
///////////////////////////////////////////////////////////////////////

// Read one full text command, parse it, and execute matching action
void handle_command_mode()
{
  // Static buffer keeps its contents between function calls
  static char cmd_buf[CMD_BUF_SIZE];
  static int cmd_index = 0;

  // Read serial input without blocking
  int ch = getchar_timeout_us(0);
  if (ch == PICO_ERROR_TIMEOUT)
  {
    return;
  }

  // Enter pressed = command complete
  if (ch == '\r' || ch == '\n')
  {
    printf("\n");

    // Ignore empty Enter
    if (cmd_index == 0)
    {
      return;
    }

    // Null-terminate the command string
    cmd_buf[cmd_index] = '\0';
    printf("CMD: %s\n", cmd_buf);

    //-----------------------------------------------------------------
    // M2 / M02 - return to manual mode
    //-----------------------------------------------------------------
    if (strcmp(cmd_buf, "M2") == 0 || strcmp(cmd_buf, "M02") == 0)
    {
      manual_mode = true;
      printf("Switched to manual mode\n");
      print_manual_menu();
    }

    //-----------------------------------------------------------------
    // M3 Snnn - spindle on with PWM value
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "M3") == 0 || strncmp(cmd_buf, "M3 ", 3) == 0)
    {
      char *s_ptr = strchr(cmd_buf, 'S');

      if (s_ptr != NULL)
      {
        int value = atoi(s_ptr + 1);

        if (value < 0)
        {
          value = 0;
        }
        if (value > 255)
        {
          value = 255;
        }

        spindle_pwm = (uint16_t)value;
        mmhal_set_spindle_pwm(spindle_pwm);
        printf("Spindle ON, PWM = %u\n", spindle_pwm);
      }
      else
      {
        printf("Error: M3 requires S value\n");
      }
    }

    //-----------------------------------------------------------------
    // M5 - spindle off
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "M5") == 0)
    {
      spindle_pwm = 0;
      mmhal_set_spindle_pwm(0);
      printf("Spindle OFF\n");
    }

    //-----------------------------------------------------------------
    // G90 - absolute positioning mode
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G90") == 0)
    {
      absolute_mode = true;
      printf("Positioning mode = ABSOLUTE\n");
    }

    //-----------------------------------------------------------------
    // G91 - relative positioning mode
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G91") == 0)
    {
      absolute_mode = false;
      printf("Positioning mode = RELATIVE\n");
    }

    //-----------------------------------------------------------------
    // G28.1 - set current position as home
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G28.1") == 0)
    {
      home_x = pos_x;
      home_y = pos_y;
      home_z = pos_z;
      printf("Home position set to X%d Y%d Z%d\n", home_x, home_y, home_z);
    }

    //-----------------------------------------------------------------
    // G28 - return to stored home position
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G28") == 0)
    {
      execute_move(home_x, home_y, home_z, "G28");
    }

    //-----------------------------------------------------------------
    // G1 / G01 - linear move, optional feed rate F
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G1") == 0 || strcmp(cmd_buf, "G01") == 0 ||
             strncmp(cmd_buf, "G1 ", 3) == 0 || strncmp(cmd_buf, "G01 ", 4) == 0)
    {
      int target_x;
      int target_y;
      int target_z;
      char *f_ptr = strchr(cmd_buf, 'F');

      parse_target_position(cmd_buf, &target_x, &target_y, &target_z);

      // Optional feed-rate parameter
      if (f_ptr != NULL)
      {
        int feed_rate = atoi(f_ptr + 1);
        set_feed_rate(feed_rate);
        printf("Feed rate = %d\n", feed_rate);
      }

      execute_move(target_x, target_y, target_z, "G1");
    }

    //-----------------------------------------------------------------
    // G2 / G02 - clockwise arc
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G2") == 0 || strcmp(cmd_buf, "G02") == 0 ||
             strncmp(cmd_buf, "G2 ", 3) == 0 || strncmp(cmd_buf, "G02 ", 4) == 0)
    {
      int target_x;
      int target_y;
      int target_z;
      char *i_ptr = strchr(cmd_buf, 'I');
      char *j_ptr = strchr(cmd_buf, 'J');

      parse_target_position(cmd_buf, &target_x, &target_y, &target_z);

      if (i_ptr == NULL || j_ptr == NULL)
      {
        printf("Error: G2 requires I and J parameters\n");
      }
      else
      {
        int i_steps = atoi(i_ptr + 1);
        int j_steps = atoi(j_ptr + 1);

        if (metric_mode)
        {
          i_steps = i_steps * X_STEPS_PER_MM;
          j_steps = j_steps * Y_STEPS_PER_MM;
        }
        else
        {
          i_steps = i_steps * 254 / 10;
          j_steps = j_steps * 254 / 10;
          i_steps = i_steps * X_STEPS_PER_MM;
          j_steps = j_steps * Y_STEPS_PER_MM;
        }

        execute_arc(target_x, target_y, i_steps, j_steps, true);
      }
    }

    //-----------------------------------------------------------------
    // G3 / G03 - counter-clockwise arc
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G3") == 0 || strcmp(cmd_buf, "G03") == 0 ||
             strncmp(cmd_buf, "G3 ", 3) == 0 || strncmp(cmd_buf, "G03 ", 4) == 0)
    {
      int target_x;
      int target_y;
      int target_z;
      char *i_ptr = strchr(cmd_buf, 'I');
      char *j_ptr = strchr(cmd_buf, 'J');

      parse_target_position(cmd_buf, &target_x, &target_y, &target_z);

      if (i_ptr == NULL || j_ptr == NULL)
      {
        printf("Error: G3 requires I and J parameters\n");
      }
      else
      {
        int i_steps = atoi(i_ptr + 1);
        int j_steps = atoi(j_ptr + 1);

        if (metric_mode)
        {
          i_steps = i_steps * X_STEPS_PER_MM;
          j_steps = j_steps * Y_STEPS_PER_MM;
        }
        else
        {
          i_steps = i_steps * 254 / 10;
          j_steps = j_steps * 254 / 10;
          i_steps = i_steps * X_STEPS_PER_MM;
          j_steps = j_steps * Y_STEPS_PER_MM;
        }

        execute_arc(target_x, target_y, i_steps, j_steps, false);
      }
    }

    //-----------------------------------------------------------------
    // G4 / G04 - dwell for P milliseconds
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G4") == 0 || strcmp(cmd_buf, "G04") == 0 ||
             strncmp(cmd_buf, "G4 ", 3) == 0 || strncmp(cmd_buf, "G04 ", 4) == 0)
    {
      char *p_ptr = strchr(cmd_buf, 'P');

      if (p_ptr != NULL)
      {
        int dwell_ms = atoi(p_ptr + 1);

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
    }

    //-----------------------------------------------------------------
    // G20 - set units to inches
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G20") == 0)
    {
      metric_mode = false;
      printf("Units = INCHES\n");
    }

    //-----------------------------------------------------------------
    // G21 - set units to millimeters
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G21") == 0)
    {
      metric_mode = true;
      printf("Units = MILLIMETERS\n");
    }

    //-----------------------------------------------------------------
    // G0 / G00 - rapid move
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "G0") == 0 || strcmp(cmd_buf, "G00") == 0 ||
             strncmp(cmd_buf, "G0 ", 3) == 0 || strncmp(cmd_buf, "G00 ", 4) == 0)
    {
      int target_x;
      int target_y;
      int target_z;

      parse_target_position(cmd_buf, &target_x, &target_y, &target_z);
      execute_move(target_x, target_y, target_z, "G0");
    }

    //-----------------------------------------------------------------
    // H / HELP - show command help
    //-----------------------------------------------------------------
    else if (strcmp(cmd_buf, "H") == 0 || strcmp(cmd_buf, "HELP") == 0)
    {
      print_command_menu();
    }

    //-----------------------------------------------------------------
    // Unknown command
    //-----------------------------------------------------------------
    else
    {
      printf("Unknown command: %s\n", cmd_buf);
    }

    // Reset buffer for next command
    cmd_index = 0;
    return;
  }

  // Backspace/delete key
  else if (ch == '\b' || ch == 127)
  {
    if (cmd_index > 0)
    {
      cmd_index--;
      printf("\b \b");
    }
  }

  // Normal input character
  else if (cmd_index < CMD_BUF_SIZE - 1)
  {
    // Store uppercase version so commands are case-insensitive
    cmd_buf[cmd_index] = (char)toupper(ch);
    cmd_index++;
    putchar(ch);
  }

  // Buffer overflow protection
  else
  {
    cmd_index = 0;
    printf("Error: command too long\n");
  }
}

///////////////////////////////////////////////////////////////////////
// Main program
///////////////////////////////////////////////////////////////////////

int main(void)
{
  // Initialise USB serial and hardware layer
  stdio_init_all();
  mmhal_init();

  printf("Init OK\n");
  print_manual_menu();

  while (true)
  {
    // Run the active control mode
    if (manual_mode)
    {
      handle_manual_mode();
    }
    else
    {
      handle_command_mode();
    }

    // Small delay to reduce CPU load
    sleep_ms(10);
  }
}