/**************************************************************
 * main.c
 * rev 3 13-Apr-2026
 * Week 6
 * PwmAdcLab
 *
 * Controls RGB LED brightness using PWM.
 * Reads the LDR using ADC.
 * Displays RGB, ADC and voltage in fixed terminal positions.
 *
 * Keyboard controls:
 *   r / R = increase / decrease red
 *   g / G = increase / decrease green
 *   b / B = increase / decrease blue
 **************************************************************/

#include "pico/stdlib.h"    /* Core Pico SDK: GPIO, timing, stdio */
#include <stdio.h>          /* printf */
#include <stdint.h>         /* uint8_t, uint16_t */
#include "hardware/pwm.h"   /* PWM peripheral */
#include "hardware/adc.h"   /* ADC peripheral */
#include "terminal.h"       /* Terminal helpers and colour constants */

/* -------- Pin assignments -------- */
#define RED_PIN     11
#define GREEN_PIN   12
#define BLUE_PIN    13
#define LDR_PIN     26      /* GPIO26 = ADC channel 0 */

/* -------- ADC constants --------
 * RP2040 ADC is 12-bit, so result ranges from 0 to 4095.
 * Reference voltage is 3.3 V.
 */
#define ADC_INPUT   0
#define ADC_MAX     4095.0f
#define ADC_REF_V   3.3f

/* -------- PWM constants --------
 * Use full 16-bit PWM range: 0 to 65535.
 * LED brightness values are stored as 0-255, then squared
 * to give a smoother visible brightness response.
 */
#define PWM_WRAP    65535

int main(void)
{
    /* Must be called before terminal input/output */
    stdio_init_all();

    /* ---------------- ADC setup ----------------
     * Initialise ADC hardware, set GPIO26 as analog input,
     * then select ADC channel 0 for the LDR.
     */
    adc_init();
    adc_gpio_init(LDR_PIN);
    adc_select_input(ADC_INPUT);

    /* ---------------- PWM setup ----------------
     * Set RGB pins to PWM function.
     */
    gpio_set_function(RED_PIN, GPIO_FUNC_PWM);
    gpio_set_function(GREEN_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BLUE_PIN, GPIO_FUNC_PWM);

    /* Find the PWM slice used by each pin */
    uint slice_r = pwm_gpio_to_slice_num(RED_PIN);
    uint slice_g = pwm_gpio_to_slice_num(GREEN_PIN);
    uint slice_b = pwm_gpio_to_slice_num(BLUE_PIN);

    /* Set PWM range for each slice */
    pwm_set_wrap(slice_r, PWM_WRAP);
    pwm_set_wrap(slice_g, PWM_WRAP);
    pwm_set_wrap(slice_b, PWM_WRAP);

    /* Enable PWM output on each slice */
    pwm_set_enabled(slice_r, true);
    pwm_set_enabled(slice_g, true);
    pwm_set_enabled(slice_b, true);

    /* RGB brightness values: 0 to 255 */
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    /* Previous ADC value used for extra-credit colour comparison */
    uint16_t prev_adc = 0;

    /* Clear terminal once, then print fixed labels */
    term_cls();
    term_move_to(1, 1);  printf("Red    :");
    term_move_to(1, 2);  printf("Green  :");
    term_move_to(1, 3);  printf("Blue   :");
    term_move_to(1, 4);  printf("ADC    :");
    term_move_to(1, 5);  printf("Voltage:");

    while (true)
    {
        /* Non-blocking keyboard read.
         * Returns PICO_ERROR_TIMEOUT if no key is waiting.
         */
        int c = getchar_timeout_us(0);

        /* -------- Adjust RGB brightness --------
         * Lowercase increases, uppercase decreases.
         * Clamp values to range 0-255.
         */
        switch (c)
        {
            case 'r': if (red < 255)   red++;   break;
            case 'R': if (red > 0)     red--;   break;

            case 'g': if (green < 255) green++; break;
            case 'G': if (green > 0)   green--; break;

            case 'b': if (blue < 255)  blue++;  break;
            case 'B': if (blue > 0)    blue--;  break;

            default: break;
        }

        /* Square each value before sending to PWM.
         * This gives a more natural visible brightness response.
         */
        pwm_set_gpio_level(RED_PIN,   red * red);
        pwm_set_gpio_level(GREEN_PIN, green * green);
        pwm_set_gpio_level(BLUE_PIN,  blue * blue);

        /* -------- Read ADC and convert to voltage -------- */
        uint16_t adc_val = adc_read();
        float voltage = ((float)adc_val * ADC_REF_V) / ADC_MAX;

        /* -------- Update RGB display -------- */
        term_move_to(10, 1);  printf("%3u", red);
        term_move_to(10, 2);  printf("%3u", green);
        term_move_to(10, 3);  printf("%3u", blue);

        /* -------- Extra credit: ADC colour change --------
         * Green = ADC increased
         * Red   = ADC decreased
         * White = unchanged
         */
        term_move_to(10, 4);

        if (adc_val > prev_adc)
        {
            term_set_color(clrGreen, clrBlack);
        }
        else if (adc_val < prev_adc)
        {
            term_set_color(clrRed, clrBlack);
        }
        else
        {
            term_set_color(clrWhite, clrBlack);
        }

        printf("%4u", adc_val);

        /* Reset terminal colour after ADC value */
        term_set_color(clrWhite, clrBlack);

        /* -------- Update voltage display -------- */
        term_move_to(10, 5);
        printf("%5.2fV", voltage);

        /* Save ADC value for next loop comparison */
        prev_adc = adc_val;

        /* Refresh at 10 Hz to reduce flicker and keep output readable */
        sleep_ms(100);
    }
}