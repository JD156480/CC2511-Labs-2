/**************************************************************
 * main.c
 * rev 1.0 18-Feb-2026 skegm
 * stepper1
 * ***********************************************************/
// runs interaction with pico board
#include "pico/stdlib.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>



// Pins for stepper motor control
#define STEP_PIN 14
#define DIR_PIN 15

//"const int" better than int by itself,
// Leave as just "int" if you intend to change it.
const int step_high = 375;
const int step_low = 375;

void init_stepper_pins()
{
  gpio_init(STEP_PIN);
  gpio_init(DIR_PIN);

  gpio_set_dir(STEP_PIN, GPIO_OUT);
  gpio_set_dir(DIR_PIN, GPIO_OUT);
}

int send_pulse_to_stepper()
{
  gpio_put(STEP_PIN, true);
  sleep_us(step_high);
  gpio_put(STEP_PIN, false);
  sleep_us(step_low);
}

int main(void)
{
  // TODO - Initialise components and variables
  stdio_init_all();
  init_stepper_pins();
  printf("GPIO has initialised\r");

  printf("Stepper Motor Control Initialised\r");
  printf("Press f for one step, g for 10 steps, h for 100 steps, or j for 200 steps.\r");
  printf("or press k for infintie spinning action!");

  bool DIR_state = false;
  bool STEP_state = false;
  bool continuous = false;

  while (true)
  {
    // Lines 69 - 75 checks if continuous flag state is high or low
    // (which is changed by pressing the k key)
    // if not pressed, runs through to next case, will run every cycle of while loop
    // cheers Terrence
    int ch = getchar_timeout_us(0);

    if (ch != PICO_ERROR_TIMEOUT)
    {

      switch (ch)
      {
      case 'f':
        // DIR_state = !DIR_state;
        //  gpio_put(DIR_PIN, DIR_state);
        send_pulse_to_stepper();
        printf("Step Pressed\n");
        break;

      case 'd':
        // DIR_state = !DIR_state;
        //  gpio_put(DIR_PIN, DIR_state);
        gpio_put(DIR_PIN, true);
        printf("Set To FORWARD\n");
        break;

      case 'a':
        // DIR_state = !DIR_state;
        //  gpio_put(DIR_PIN, DIR_state);
        gpio_put(DIR_PIN, false);
        printf("Set To BACKWARDS\n");
        break;

      case 'g':
        for (int i = 0; i < 10; i++)
        {
          send_pulse_to_stepper();
        }

        printf("10 Steps\n");
        break;

      case 'h':
        for (int i = 0; i < 100; i++)
        {
          send_pulse_to_stepper();
        }

        printf("200 Steps\n");
        break;

      case 'j':
        for (int i = 0; i < 200; i++)
        {
          send_pulse_to_stepper();
        }

        printf("200 Steps!!!! Wow!\n");
        break;

      case 'k':
        continuous != continuous;
        {
          printf("Infinite Steps!!!! Woweee!\n");
          break;
        }
      default:
        printf("Unknown input\r\n");
        break;
      }
    }
    if (continuous)
    {
      send_pulse_to_stepper();
    }
  }
}