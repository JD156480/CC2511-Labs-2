/**************************************************************
 * main.c
 * rev 1.4 06-Apr-2026
 * PwmAdcLab
 *
 * Controls RGB LED brightness using PWM.
 * Reads the LDR using ADC.
 * Displays RGB, ADC and voltage in fixed terminal positions.
 **************************************************************/

#include "pico/stdlib.h"    /* Core Pico SDK: GPIO, timing, stdio support */
#include <stdio.h>          /* Standard formatted output: printf */
#include "hardware/pwm.h"   /* PWM peripheral */
#include "hardware/adc.h"   /* ADC peripheral */
#include "terminal.h"       /* Terminal helpers and colour constants */

/* Pin assignments */
#define RED_PIN     11
#define GREEN_PIN   12
#define BLUE_PIN    13
#define LDR_PIN     26      /* GPIO26 = ADC channel 0 */

/* ADC constants: 12-bit ADC, Vref = 3.3 V */
#define ADC_INPUT   0
#define ADC_MAX     4095.0f
#define ADC_REF_V   3.3f

int main(void)
{
    /* Must be called before terminal input/output */
    stdio_init_all();

    /* --- ADC setup --- */
    adc_init();                     /* Initialise ADC hardware */
    adc_gpio_init(LDR_PIN);         /* Set GPIO26 as analog input */
    adc_select_input(ADC_INPUT);    /* Select ADC channel 0 */

    /* --- PWM setup --- */
    gpio_set_function(RED_PIN, GPIO_FUNC_PWM);
    gpio_set_function(GREEN_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BLUE_PIN, GPIO_FUNC_PWM);

    /* Find the PWM slice used by each LED pin */
    uint slice_r = pwm_gpio_to_slice_num(RED_PIN);
    uint slice_g = pwm_gpio_to_slice_num(GREEN_PIN);
    uint slice_b = pwm_gpio_to_slice_num(BLUE_PIN);

    /* Full 16-bit PWM range: 0 to 65535 */
    pwm_set_wrap(slice_r, 65535);
    pwm_set_wrap(slice_g, 65535);
    pwm_set_wrap(slice_b, 65535);

    pwm_set_enabled(slice_r, true);
    pwm_set_enabled(slice_g, true);
    pwm_set_enabled(slice_b, true);

    /* RGB brightness values: 0 to 255 */
    int red = 0;
    int green = 0;
    int blue = 0;

    /* Previous ADC value for extra-credit colour display */
    uint16_t prev_adc = 0;

    /* Clear screen once, then print labels */
    term_cls();

    term_move_to(1, 1);  printf("Red    :");
    term_move_to(1, 2);  printf("Green  :");
    term_move_to(1, 3);  printf("Blue   :");
    term_move_to(1, 4);  printf("ADC    :");
    term_move_to(1, 5);  printf("Voltage:");

    while (true)
    {
        /* Non-blocking keyboard read */
        int c = getchar_timeout_us(0);

        /* Adjust RGB levels */
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

        /* Square values for better brightness response */
        pwm_set_gpio_level(RED_PIN, red * red);
        pwm_set_gpio_level(GREEN_PIN, green * green);
        pwm_set_gpio_level(BLUE_PIN, blue * blue);

        /* Read ADC and convert to voltage */
        uint16_t adc_val = adc_read();
        float voltage = ((float)adc_val * ADC_REF_V) / ADC_MAX;

        /* Update RGB values */
        term_move_to(10, 1);  printf("%3d", red);
        term_move_to(10, 2);  printf("%3d", green);
        term_move_to(10, 3);  printf("%3d", blue);

        /* Extra credit: ADC colour shows rise/fall */
        term_move_to(10, 4);
        if (adc_val > prev_adc)
            term_set_color(clrGreen, clrBlack);
        else if (adc_val < prev_adc)
            term_set_color(clrRed, clrBlack);
        else
            term_set_color(clrWhite, clrBlack);

        printf("%4u", adc_val);
        term_set_color(clrWhite, clrBlack);   /* Reset colour */

        /* Update voltage */
        term_move_to(10, 5);  printf("%4.2fV", voltage);

        prev_adc = adc_val;

        /* Slow refresh to keep display readable */
        sleep_ms(100);
    }
}