/**************************************************************
 * rgb_test.c
 * Simple RGB LED hardware test for CC2511 Dev Board
 *
 * Cycles:
 *   Red   -> off
 *   Green -> off
 *   Blue  -> off
 *   White -> off
 *
 * If any colour does not light here, suspect hardware damage
 * or wrong board / wrong pin map.
 **************************************************************/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

/* CC2511 Dev Board RGB pins from schematic */
#define RED_PIN    11
#define GREEN_PIN  12
#define BLUE_PIN   13

/* Use full 16-bit PWM range */
#define PWM_WRAP   65535

static void init_rgb_pwm(void)
{
    const int pins[3] = {RED_PIN, GREEN_PIN, BLUE_PIN};

    for (int i = 0; i < 3; i++)
    {
        gpio_set_function(pins[i], GPIO_FUNC_PWM);

        uint slice = pwm_gpio_to_slice_num(pins[i]);
        pwm_set_wrap(slice, PWM_WRAP);
        pwm_set_enabled(slice, true);
    }

    /* Start with all off */
    pwm_set_gpio_level(RED_PIN, 0);
    pwm_set_gpio_level(GREEN_PIN, 0);
    pwm_set_gpio_level(BLUE_PIN, 0);
}

static void set_rgb(uint16_t r, uint16_t g, uint16_t b)
{
    pwm_set_gpio_level(RED_PIN, r);
    pwm_set_gpio_level(GREEN_PIN, g);
    pwm_set_gpio_level(BLUE_PIN, b);
}

int main(void)
{
    stdio_init_all();
    init_rgb_pwm();

    while (true)
    {
        /* Red */
        set_rgb(PWM_WRAP, 0, 0);
        sleep_ms(2000);
        set_rgb(0, 0, 0);
        sleep_ms(500);

        /* Green */
        set_rgb(0, PWM_WRAP, 0);
        sleep_ms(2000);
        set_rgb(0, 0, 0);
        sleep_ms(500);

        /* Blue */
        set_rgb(0, 0, PWM_WRAP);
        sleep_ms(2000);
        set_rgb(0, 0, 0);
        sleep_ms(500);

        /* White */
        set_rgb(PWM_WRAP, PWM_WRAP, PWM_WRAP);
        sleep_ms(2000);
        set_rgb(0, 0, 0);
        sleep_ms(1000);
    }
}