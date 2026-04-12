/**************************************************************
 * main.c
 * rev 1.0 12-Feb-2026 skegm
 * GpioLab (Week 3)
 *
 * Controls 3 LEDs, or RGB using GPIO.
 * Receives commands from the serial terminal:
 *   r/R = toggle red LED
 *   g/G = toggle green LED
 *   b/B = toggle blue LED
 * Prints LED status message/display descriptive status reports
 * via terminal output. (useful for manual mode input handling)
 *************************************************************/

#include "pico/stdlib.h" // gpio_init, gpio_set_dir, gpio_put, sleep_ms
#include <stdbool.h>     // bool, true, false (C99+)
#include <stdio.h>       // printf, getchar_timeout_us

// GPIO pin numbers 
#define RED_LED   11
#define GREEN_LED 12
#define BLUE_LED  13

int main(void)
{
    // Initialise USB serial (required before printf/getchar)
    stdio_init_all();

    // Initialise each LED pin as an output
    gpio_init(RED_LED);
    gpio_set_dir(RED_LED, GPIO_OUT);

    gpio_init(GREEN_LED);
    gpio_set_dir(GREEN_LED, GPIO_OUT);

    gpio_init(BLUE_LED);
    gpio_set_dir(BLUE_LED, GPIO_OUT);

    // Track LED states (false = OFF, true = ON)
    bool red_state   = false;
    bool green_state = false;
    bool blue_state  = false;

    printf("Press r, g, or b to toggle LEDs\r\n");

    while (true)
    {
        // Non-blocking read — returns PICO_ERROR_TIMEOUT if no key pressed
        int ch = getchar_timeout_us(0);

        if (ch != PICO_ERROR_TIMEOUT)
        {
            switch (ch)
            {
            case 'r':
            case 'R':
                red_state = !red_state;            // toggle state
                gpio_put(RED_LED, red_state);       // apply to hardware
                printf("Red LED %s\r\n", red_state ? "ON" : "OFF");
                break;

            case 'g':
            case 'G':
                green_state = !green_state;
                gpio_put(GREEN_LED, green_state);
                printf("Green LED %s\r\n", green_state ? "ON" : "OFF");
                break;

            case 'b':
            case 'B':
                blue_state = !blue_state;
                gpio_put(BLUE_LED, blue_state);
                printf("Blue LED %s\r\n", blue_state ? "ON" : "OFF");
                break;

            case '\r':
            case '\n':
                break; // ignore Enter key

            default:
                printf("Unknown input\r\n");
                break;
            }
        }
//optional small delay
        sleep_ms(20); // yield CPU briefly; avoids spinning at 100%
    }
}