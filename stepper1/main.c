/**************************************************************
 * main.c
 * Week 4 Lab
 * stepper1
 *
 * Reads terminal commands and controls a DRV8825 stepper driver.
 *
 * Required commands:
 *   d = set forward direction
 *   a = set reverse direction
 *   f = execute 1 step
 *   g = execute 10 steps
 *   h = execute 100 steps
 *   j = execute 200 steps
 *
 * Optional commands:
 *   0/1/2/3/4/5 = set microstepping mode
 *
 * Extra feature:
 *   k = toggle continuous stepping on/off
 ***************************************************************/

#include "pico/stdlib.h"
#include <stdbool.h>
#include <stdio.h>

/* GPIO pins for DRV8825 control — update to match your wiring */
#define STEP_PIN 14
#define DIR_PIN  15

/* Optional DRV8825 microstepping pins (MODE0, MODE1, MODE2) */
#define M0_PIN   18
#define M1_PIN   19
#define M2_PIN   20

/* Step pulse timing in microseconds
 * DRV8825 requires STEP high and low >= 1.9 us.
 * low_delay_us is chosen to keep step rate within motor limits.
 */
int high_delay_us = 2;
int low_delay_us  = 740;

/* Initialise GPIO pins used by the stepper driver */
void init_stepper_pins(void)
{
    gpio_init(STEP_PIN);
    gpio_set_dir(STEP_PIN, GPIO_OUT);

    gpio_init(DIR_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);

    gpio_init(M0_PIN);
    gpio_set_dir(M0_PIN, GPIO_OUT);

    gpio_init(M1_PIN);
    gpio_set_dir(M1_PIN, GPIO_OUT);

    gpio_init(M2_PIN);
    gpio_set_dir(M2_PIN, GPIO_OUT);

    /* Default outputs */
    gpio_put(STEP_PIN, 0);
    gpio_put(DIR_PIN, 0);

    /* Default microstepping = full step */
    gpio_put(M0_PIN, 0);
    gpio_put(M1_PIN, 0);
    gpio_put(M2_PIN, 0);
}

/* Send one STEP pulse to the DRV8825 */
void send_pulse_to_stepper(void)
{
    gpio_put(STEP_PIN, 1);
    sleep_us(high_delay_us);
    gpio_put(STEP_PIN, 0);
    sleep_us(low_delay_us);
}

/* Execute a specified number of steps */
void execute_n_steps(int steps)
{
    for (int i = 0; i < steps; i++)
    {
        send_pulse_to_stepper();
    }
}

/* Set stepper direction: true = forward, false = reverse */
void set_stepper_direction(bool forward)
{
    gpio_put(DIR_PIN, forward);
    sleep_us(1);   // Allow DIR to settle before next STEP rising edge
}

/* Set DRV8825 microstepping mode using MODE0, MODE1, MODE2 */
void set_microstep_mode(bool m0, bool m1, bool m2)
{
    gpio_put(M0_PIN, m0);
    gpio_put(M1_PIN, m1);
    gpio_put(M2_PIN, m2);
    sleep_us(10);  // Allow mode pins to settle
}

int main(void)
{
    stdio_init_all();
    init_stepper_pins();

    bool continuous_mode = false;

    printf("Stepper Motor Control Initialized\r\n");

    while (true)
    {
        int ch = getchar_timeout_us(0);

        if (ch != PICO_ERROR_TIMEOUT)
        {
            switch (ch)
            {
            /* Required commands from lab sheet */
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

            /* Optional microstepping commands */
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

            /* Extra feature: continuous mode toggle */
            case 'k':
                continuous_mode = !continuous_mode;
                printf("Continuous Mode %s\r\n",
                       continuous_mode ? "ON" : "OFF");
                break;

            case '\r':
            case '\n':
                break;  // Ignore Enter key

            default:
                printf("Unknown Command '%c'\r\n", (char)ch);
                break;
            }
        }

        if (continuous_mode)
        {
            send_pulse_to_stepper();
        }
    }
}