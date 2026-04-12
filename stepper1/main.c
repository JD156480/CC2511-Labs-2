/**************************************************************
 * main.c
 * rev 1.0 18-Feb-2026 skegm
 * stepper1
 * Reads terminal commands and controls a DRV8825 stepper driver.
 * d = forward, a = reverse
 * f/g/h/j = 1/10/100/200 steps
 *
 * Optional:
 * 0/1/2/3/4/5 = set microstepping mode
 * ***********************************************************/
#include "pico/stdlib.h"
#include <stdbool.h>
#include <stdio.h>

/* --- Pin Definitions ---
 * #define is resolved at compile time — better than variables for fixed hardware pins.
 * Place outside main() so all functions can see them (global scope). */
#define STEP_PIN 14 // DRV8825 STEP input
#define DIR_PIN 15  // DRV8825 DIR input

// Optional: microstepping mode pins (M0/M1/M2 on DRV8825)
#define M0_PIN 18
#define M1_PIN 19
#define M2_PIN 20

/* --- Step Pulse Timing ---
 * Use plain int (not const) so values can be adjusted at runtime during testing.
 * high_delay_us: DRV8825 min STEP high time = 1.9 µs → use 2 µs.
 * low_delay_us:  controls speed. 400 rpm max → 200 steps/rev → ~1333 steps/s
 *                → min total step period ≈ 750 µs, so low ≈ 748 µs. Use 740 µs. */
int high_delay_us = 2;  // STEP pin high time in microseconds
int low_delay_us = 740; // STEP pin low time in microseconds (sets motor speed)

/* Initialise all stepper GPIO pins as outputs.
 * Default: full-step mode (M0=M1=M2=0). */
void init_stepper_pins(void)
{
  gpio_init(STEP_PIN);
  gpio_init(DIR_PIN);
  gpio_init(M0_PIN);
  gpio_init(M1_PIN);
  gpio_init(M2_PIN);

  gpio_set_dir(STEP_PIN, GPIO_OUT);
  gpio_set_dir(DIR_PIN, GPIO_OUT);
  gpio_set_dir(M0_PIN, GPIO_OUT);
  gpio_set_dir(M1_PIN, GPIO_OUT);
  gpio_set_dir(M2_PIN, GPIO_OUT);

  gpio_put(M0_PIN, 0); // Full-step: M0=0, M1=0, M2=0
  gpio_put(M1_PIN, 0);
  gpio_put(M2_PIN, 0);
}

/* Send one step pulse to the DRV8825.
 * Rising edge on STEP advances the motor one microstep.
 * Pulse shape defined by high_delay_us and low_delay_us. */
void send_pulse_to_stepper(void)
{
  gpio_put(STEP_PIN, true);
  sleep_us(high_delay_us); // Hold high for required pulse width
  gpio_put(STEP_PIN, false);
  sleep_us(low_delay_us); // Low period determines step rate
}

/* Move the motor a specified number of steps.
 * Called by case handlers for f/g/h/j. */
void execute_n_steps(int steps)
{
  for (int i = 0; i < steps; i++)
  {
    send_pulse_to_stepper();
  }
}

/* Set motor direction. forward=true → forward, forward=false → reverse.
 * DRV8825 requires DIR to be stable before the next STEP pulse (min 650 ns setup time). */
void set_stepper_direction(bool forward)
{
  gpio_put(DIR_PIN, forward);
  sleep_us(1); // Wait for DIR pin to be registered by DRV8825
}

/* Optional: set microstepping resolution via M0/M1/M2 pins.
 * DRV8825 truth table: 000=full, 100=1/2, 010=1/4, 110=1/8, 001=1/16, 101=1/32 */
void set_microstep_mode(bool m0, bool m1, bool m2)
{
  gpio_put(M0_PIN, m0);
  gpio_put(M1_PIN, m1);
  gpio_put(M2_PIN, m2);
  sleep_us(10); // Allow DRV8825 time to register the new mode
}

int main(void)
{
  stdio_init_all();
  init_stepper_pins();
  printf("Stepper Motor Control Initialized\r\n");

  bool continuous = false; // Optional continuous spin toggle

  while (true)
  {
    /* Non-blocking read — returns PICO_ERROR_TIMEOUT if no key pressed.
     * This lets the loop continue running (e.g. for continuous mode). */
    int ch = getchar_timeout_us(0);

    if (ch != PICO_ERROR_TIMEOUT)
    {
      switch (ch)
      {
      /* --- Required cases (Lab Table 1) --- */
      case 'd':
        set_stepper_direction(true);
        printf("Direction: Forward\r\n");
        break;

      case 'a':
        set_stepper_direction(false);
        printf("Direction: Reverse\r\n");
        break;

      case 'f':
        execute_n_steps(1);
        printf("1 Step Executed\r\n");
        break;

      case 'g':
        execute_n_steps(10);
        printf("10 Steps Executed\r\n");
        break;

      case 'h':
        execute_n_steps(100);
        printf("100 Steps Executed\r\n");
        break;

      case 'j':
        execute_n_steps(200);
        printf("200 Steps Executed\r\n");
        break;

      /* --- Optional: microstepping mode selection (Lab Table 2) --- */
      case '0':
        set_microstep_mode(0, 0, 0);
        printf("Microstep/step = 1\r\n");
        break;

      case '1':
        set_microstep_mode(1, 0, 0);
        printf("Microstep/step = 2\r\n");
        break;

      case '2':
        set_microstep_mode(0, 1, 0);
        printf("Microstep/step = 4\r\n");
        break;

      case '3':
        set_microstep_mode(1, 1, 0);
        printf("Microstep/step = 8\r\n");
        break;

      case '4':
        set_microstep_mode(0, 0, 1);
        printf("Microstep/step = 16\r\n");
        break;

      case '5':
        set_microstep_mode(1, 0, 1);
        printf("Microstep/step = 32\r\n");
        break;

      /* --- Optional: continuous spin toggle --- */
      case 'k':
        continuous = !continuous;
        printf(continuous ? "Continuous mode ON\r\n" : "Continuous mode OFF\r\n");
        break;

      /* --- Default: report unknown character (Lab Table 1) ---
       * Cast ch to char so %c prints the actual character pressed. */
      default:
        printf("Unknown Command '%c'\r\n", (char)ch);
        break;
      }
    }

    // Optional: send one pulse per loop iteration if continuous mode is active
    if (continuous)
    {
      send_pulse_to_stepper();
    }
  }
}