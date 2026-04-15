/****************************************************
 * mmhal.c
 * rev 1.0 07-April-2026 CNS02 Adam, Faye
 * milling_basic
 *
 * Milling Machine Hardware Access Layer
 * This file contains the low-level hardware control
 * used by main.c.
 * 
 * Analogy for memory: This is the ships hardware
 * (engine, electronics etc)
 * 
 ****************************************************/
#include "mmhal.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#ifndef CNC_VERSION
#define CNC_VERSION 2
#endif

// STEP and DIR pin arrays for X, Y and Z axes
const int step_pins[] = {XSTEP_PIN, YSTEP_PIN, ZSTEP_PIN};
const int dir_pins[]  = {XDIR_PIN, YDIR_PIN, ZDIR_PIN};

// Direction multipliers compensate for board/mechanical orientation
#if CNC_VERSION == 1
const int stepper_multipliers[] = {-1, 1, -1};
#elif CNC_VERSION == 2
const int stepper_multipliers[] = {1, -1, 1};
#else
#error "Invalid CNC_VERSION"
#endif

// Step pulse timing in microseconds
volatile int mmhal_high_delay_us = 5;  // minimum per DRV8825 datasheet is 2, 5 to be safe 
volatile int mmhal_low_delay_us  = 745; // total period ~750 µs for 400 RPM

/**
 * @brief Initialise GPIO and PWM hardware.
 */
// Adam
void mmhal_init()
{
  // ENABLE is active-low: writing 0 turns the drivers ON
  gpio_init(ENABLE_PIN);
  gpio_set_dir(ENABLE_PIN, GPIO_OUT);
  gpio_put(ENABLE_PIN, 0);

  // Initialise STEP and DIR pins for all three axes
  for (int i = 0; i < DIMCOUNT; i++)
  {
    gpio_init(step_pins[i]);
    gpio_set_dir(step_pins[i], GPIO_OUT);
    gpio_put(step_pins[i], 0);  // idle low

    gpio_init(dir_pins[i]);
    gpio_set_dir(dir_pins[i], GPIO_OUT);
    gpio_put(dir_pins[i], 0);   // default direction
  }

  // X-axis DRV8825 microstepping mode pins
  gpio_init(X_MODE0_PIN); gpio_set_dir(X_MODE0_PIN, GPIO_OUT); gpio_put(X_MODE0_PIN, 0);
  gpio_init(X_MODE1_PIN); gpio_set_dir(X_MODE1_PIN, GPIO_OUT); gpio_put(X_MODE1_PIN, 0);
  gpio_init(X_MODE2_PIN); gpio_set_dir(X_MODE2_PIN, GPIO_OUT); gpio_put(X_MODE2_PIN, 0);

  // Y-axis DRV8825 microstepping mode pins
  gpio_init(Y_MODE0_PIN); gpio_set_dir(Y_MODE0_PIN, GPIO_OUT); gpio_put(Y_MODE0_PIN, 0);
  gpio_init(Y_MODE1_PIN); gpio_set_dir(Y_MODE1_PIN, GPIO_OUT); gpio_put(Y_MODE1_PIN, 0);
  gpio_init(Y_MODE2_PIN); gpio_set_dir(Y_MODE2_PIN, GPIO_OUT); gpio_put(Y_MODE2_PIN, 0);

  // Fault pins are inputs - DRV8825 pulls these LOW on a fault
  // enable Pico's internal pull-up so pin doesnt float, gives accurate readings
  gpio_init(XFLT_PIN); gpio_set_dir(XFLT_PIN, GPIO_IN);
  gpio_pull_up(XFLT_PIN); // keep input high when no fault is present
  gpio_init(YFLT_PIN); gpio_set_dir(YFLT_PIN, GPIO_IN);
  gpio_pull_up(YFLT_PIN);
  gpio_init(ZFLT_PIN); gpio_set_dir(ZFLT_PIN, GPIO_IN);
  gpio_pull_up(ZFLT_PIN);
  // Spindle PWM setup
  gpio_set_function(SPINDLE_PIN, GPIO_FUNC_PWM);  // switch pin from GPIO to PWM mode

  uint slice_num = pwm_gpio_to_slice_num(SPINDLE_PIN);  // find which PWM block owns this pin
  uint chan_num  = pwm_gpio_to_channel(SPINDLE_PIN);

  pwm_set_wrap(slice_num, 255);               // counter range 0-255 = 256 speed levels
  pwm_set_chan_level(slice_num, chan_num, 0);  // spindle off at startup
  pwm_set_enabled(slice_num, true);

  // Apply default full-step mode to both axes
  mmhal_set_microstepping(XDIM, MMHAL_MS_MODE_1);
  mmhal_set_microstepping(YDIM, MMHAL_MS_MODE_1);
  // Z-axis has no microstepping pins on this board (fixed full-step)
}


/**
 * @brief Set spindle PWM duty cycle.
 * @param pwm_level Duty cycle value in range 0-255.
 */
// Faye
void mmhal_set_spindle_pwm(uint16_t pwm_level)
{
  uint slice = pwm_gpio_to_slice_num(SPINDLE_PIN);
  uint chan  = pwm_gpio_to_channel(SPINDLE_PIN);

  // Clamp duty cycle to PWM range
  if (pwm_level > 255)
  {
    pwm_level = 255;
  }

  pwm_set_chan_level(slice, chan, pwm_level);
}


/**
 * @brief Set X or Y microstepping mode on the DRV8825.
 *
 * DRV8825 MODE pin truth table (MODE2:MODE1:MODE0):
 *   0 0 0 = full step  | 0 1 1 = 1/8
 *   0 0 1 = half step  | 1 0 0 = 1/16
 *   0 1 0 = 1/4 step   | 1 0 1 = 1/32
 *
 * @param x_or_y XDIM or YDIM
 * @param mode   mmhal_microstep_mode_t value
 */
// Adam
void mmhal_set_microstepping(int x_or_y, mmhal_microstep_mode_t mode)
{
  int mode0_pin, mode1_pin, mode2_pin;

  // Point at the correct axis's mode pins
  if (x_or_y == XDIM)
  {
    mode0_pin = X_MODE0_PIN;
    mode1_pin = X_MODE1_PIN;
    mode2_pin = X_MODE2_PIN;
  }
  else if (x_or_y == YDIM)
  {
    mode0_pin = Y_MODE0_PIN;
    mode1_pin = Y_MODE1_PIN;
    mode2_pin = Y_MODE2_PIN;
  }
  else
  {
    return;  // invalid axis
  }

  // Write MODE2:MODE1:MODE0 pins for the requested step size
  switch (mode)
  {
    case MMHAL_MS_MODE_1:  gpio_put(mode0_pin, 0); gpio_put(mode1_pin, 0); gpio_put(mode2_pin, 0); break;
    case MMHAL_MS_MODE_2:  gpio_put(mode0_pin, 1); gpio_put(mode1_pin, 0); gpio_put(mode2_pin, 0); break;
    case MMHAL_MS_MODE_4:  gpio_put(mode0_pin, 0); gpio_put(mode1_pin, 1); gpio_put(mode2_pin, 0); break;
    case MMHAL_MS_MODE_8:  gpio_put(mode0_pin, 1); gpio_put(mode1_pin, 1); gpio_put(mode2_pin, 0); break;
    case MMHAL_MS_MODE_16: gpio_put(mode0_pin, 0); gpio_put(mode1_pin, 0); gpio_put(mode2_pin, 1); break;
    case MMHAL_MS_MODE_32: gpio_put(mode0_pin, 1); gpio_put(mode1_pin, 0); gpio_put(mode2_pin, 1); break;
    default: return;  // invalid mode
  }

  sleep_us(10);  // let MODE pins settle before next step
}

/* Adam - Bitwise version kept for learning only.
 * Works because enum values 0-5 happen to be the 3-bit binary
 * encoding of the MODE pins directly:
 * MODE_1=0=0b000, MODE_2=1=0b001, MODE_4=2=0b010 ... etc.
 * So we can extract each bit with shifts and masks instead of a switch.
void mmhal_set_microstepping(int x_or_y, mmhal_microstep_mode_t mode)
{
  int mode0_pin;
  int mode1_pin;
  int mode2_pin;

  if (x_or_y == XDIM)
  {
    mode0_pin = X_MODE0_PIN;
    mode1_pin = X_MODE1_PIN;
    mode2_pin = X_MODE2_PIN;
  }
  else if (x_or_y == YDIM)
  {
    mode0_pin = Y_MODE0_PIN;
    mode1_pin = Y_MODE1_PIN;
    mode2_pin = Y_MODE2_PIN;
  }
  else
  {
    return;
  }

  gpio_put(mode0_pin,  mode        & 1); //bit 0 of mode value
  gpio_put(mode1_pin, (mode >> 1)  & 1); //bit 1
  gpio_put(mode2_pin, (mode >> 2)  & 1); //bit 2

  sleep_us(10);
}
*/

/**
 * @brief Step selected motors once.
 * @param dirs Array of directions:
 *        -1 = negative, 0 = no move, 1 = positive
 */
// Faye
static void mmhal_step_motors_impl(int dirs[])
{
  bool do_step[DIMCOUNT] = {false, false, false};

  // Set direction pins first
  for (int i = 0; i < DIMCOUNT; i++)
  {
    int actual_dir = dirs[i] * stepper_multipliers[i];

    if (actual_dir < 0)
    {
      gpio_put(dir_pins[i], 0);
      do_step[i] = true;
    }
    else if (actual_dir > 0)
    {
      gpio_put(dir_pins[i], 1);
      do_step[i] = true;
    }
  }

  // Allow DIR pins time to settle before STEP pulse
  sleep_us(10);

  // Raise STEP pins for requested axes
  for (int i = 0; i < DIMCOUNT; i++)
  {
    if (do_step[i])
    {
      gpio_put(step_pins[i], 1);
    }
  }

  sleep_us(mmhal_high_delay_us);

  // Lower STEP pins
  for (int i = 0; i < DIMCOUNT; i++)
  {
    if (do_step[i])
    {
      gpio_put(step_pins[i], 0);
    }
  }

  sleep_us(mmhal_low_delay_us);
}

/**
 * @brief Public wrapper for stepping X, Y and Z motors.
 */
// Adam
void mmhal_step_motors(int x_dir, int y_dir, int z_dir)
{
  int dirs[3] = {x_dir, y_dir, z_dir};
  mmhal_step_motors_impl(dirs);
}