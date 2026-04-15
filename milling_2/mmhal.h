/**********************************************
 * mmhal.h
 * rev 1.0 07-April-2026 CNS02 Adam, Faye
 * milling_basic
 * 
 * Milling Machine Hardware Access Layer
 * "mmhal.h" is a header file, which contains:
 * A promise sheet/menu of shared definitions
 * and function declarations, used by "mmhal.c"
 * 
 * Analogy for memory: This is the ship controls/cockpit.
 *
 **********************************************/

#ifndef __MMHAL_H__
#define __MMHAL_H__

#include <stdint.h>

///////////////////////////////////////////////////////////////////////
// Pins
///////////////////////////////////////////////////////////////////////

/**
 * Pin definitions for the CNC controller. These are the GPIO pin
 * numbers on the microcontroller that are used for various functions
 * in the CNC controller.
 */
enum
{
  SERIAL_TX_PIN = 0,
  SERIAL_RX_PIN = 1,
  X_MODE0_PIN = 2,
  X_MODE1_PIN = 3,
  X_MODE2_PIN = 4,
  XSTEP_PIN = 5,
  XDIR_PIN = 6,
  XFLT_PIN = 7,
  Y_MODE0_PIN = 8,
  Y_MODE1_PIN = 9,
  Y_MODE2_PIN = 10,
  YSTEP_PIN = 11,
  YDIR_PIN = 12,
  YFLT_PIN = 13,
  ZSTEP_PIN = 14,
  ZDIR_PIN = 15,
  ZFLT_PIN = 16,
  SPINDLE_PIN = 17,
  PUSH1_PIN = 18,
  GPIO19_PIN = 19,
  GPIO20_PIN = 20,
  GPIO21_PIN = 21,
  ENABLE_PIN = 22,
  GPIO26_PIN = 26,
  GPIO27_PIN = 27,
  GPIO28_PIN = 28
};

///////////////////////////////////////////////////////////////////////
// Stepper motors
///////////////////////////////////////////////////////////////////////

enum DIMS
{
  XDIM = 0,
  YDIM,
  ZDIM,
  DIMCOUNT
};
extern const int step_pins[];
extern const int dir_pins[];

extern volatile int mmhal_high_delay_us; // Microseconds for step pulse high time
extern volatile int mmhal_low_delay_us;  // Microseconds for step pulse low time

typedef enum
{
  MMHAL_MS_MODE_1 = 0,
  MMHAL_MS_MODE_2,
  MMHAL_MS_MODE_4,
  MMHAL_MS_MODE_8,
  MMHAL_MS_MODE_16,
  MMHAL_MS_MODE_32
} mmhal_microstep_mode_t;

/**
 *
 * @brief Initialise all GPIO pins, DRV8825 drivers, and spindle PWM.
 * 
 */
// Faye
void mmhal_init();

/**
 * @brief Run motors in specified directions
 * @param x_dir -1 for negative, 0 for no movement, 1 for positive
 * @param y_dir -1 for negative, 0 for no movement, 1 for positive
 * @param z_dir -1 for negative, 0 for no movement, 1 for positive
 */
void mmhal_step_motors(int x_dir, int y_dir, int z_dir);

/**
 * @brief Set microstepping mode
 * @param x_or_y  XDIM for X axis, YDIM for Y axis
 * @param mode    Microstepping mode to set (MMHAL_MS_MODE_1 through _32)
 */
void mmhal_set_microstepping(int x_or_y, mmhal_microstep_mode_t mode);

/**
 * @brief Set spindle PWM level
 * @param pwm_level Duty cycle value in range 0-255
 */
void mmhal_set_spindle_pwm(uint16_t pwm_level);

#endif // __MMHAL_H__