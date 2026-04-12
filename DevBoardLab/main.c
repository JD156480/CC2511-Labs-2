/**************************************************************
 * main.c
 * rev 1.0 06-Apr-2026
 * DevBoardLab – Week 7/8 Lab, Parts 1–5
 *
 * Event-driven terminal UI for the CC2511 Dev Board.
 *
 * Design pattern:
 *   ISR/callback -> capture event, set flag, do no heavy work
 *   main loop    -> process flags, update outputs/display, sleep
 *
 * This follows the interrupt/event-loop style taught in class.
 **************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

#include "terminal.h"

/* ────────────────────────────────────────────────────────── */
/* UART configuration                                        */
/* ────────────────────────────────────────────────────────── */
#define UART_ID      uart0
#define BAUD_RATE    115200
#define UART_TX_PIN  0
#define UART_RX_PIN  1

/* ────────────────────────────────────────────────────────── */
/* Shared UART buffer                                        */
/* ────────────────────────────────────────────────────────── */
#define UART_BUF_LEN 32

/* ────────────────────────────────────────────────────────── */
/* Screen layout                                             */
/* ────────────────────────────────────────────────────────── */
#define COMMAND_COL   2
#define COMMAND_ROW   2

#define LABEL_COL     2
#define VALUE_COL     12

#define RED_ROW       4
#define GREEN_ROW     5
#define BLUE_ROW      6

#define PUSH_ROW_TOP  8

#define LIGHT_ROW     12
#define TEMP_ROW      13

/* Start typed input after "Next command: " */
#define INPUT_ROW     14
#define INPUT_COL     16

/* ────────────────────────────────────────────────────────── */
/* Dev board pin mapping                                     */
/* ────────────────────────────────────────────────────────── */
#define PUSH_PIN_COUNT   3
#define PUSH1_PIN        2
#define PUSH2_PIN        3
#define PUSH3_PIN        4

#define RED_PIN          11
#define GREEN_PIN        12
#define BLUE_PIN         13

#define LDR_ADC_PIN      26
#define LDR_ADC_CHANNEL  0
#define TEMP_ADC_CHANNEL 4

/* Refresh rate: 100 ms = 10 Hz */
#define TIMER_INTERVAL_MS 100

/* ────────────────────────────────────────────────────────── */
/* Shared state: ISR/callback <-> main loop                  */
/* volatile is required because these can change at any time */
/* outside normal program flow.                              */
/* ────────────────────────────────────────────────────────── */
volatile bool uart_command_received = false;
volatile char uart_command_buffer[UART_BUF_LEN];
volatile int  uart_command_index = 0;

int push_pins[PUSH_PIN_COUNT] = {PUSH1_PIN, PUSH2_PIN, PUSH3_PIN};
volatile bool push_states[PUSH_PIN_COUNT] = {true, true, true};
volatile bool push_state_changed = true;

struct repeating_timer timer;
volatile bool timer_ticked = true;

/* ────────────────────────────────────────────────────────── */
/* Normal program state                                      */
/* ────────────────────────────────────────────────────────── */
uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;

uint16_t light_adc_value = 0;
uint16_t temp_adc_value = 0;

/* ────────────────────────────────────────────────────────── */
/* Function prototypes                                       */
/* ────────────────────────────────────────────────────────── */
void on_uart_rx(void);
void init_uart(void);

void display_cursor(void);
void clear_input(void);
void display_background(void);
void display_command(const char *command);

void init_rgb_led(void);
void set_rgb_led(uint8_t r, uint8_t g, uint8_t b);
void display_rgb_labels(void);
void display_rgb_values(uint8_t r, uint8_t g, uint8_t b);
bool process_command(const char *command);

void init_push_buttons(void);
void on_gpio_change(uint gpio, uint32_t events);
void display_push_labels(void);
void display_push_values(const volatile bool *states);

void init_timer(void);
bool on_timer(struct repeating_timer *t);

void init_adc(void);
bool read_adc_values(void);
void display_adc_labels(void);
void display_adc_values(void);

/* ────────────────────────────────────────────────────────── */
/* UART RX interrupt handler                                 */
/* Called when characters arrive on UART0.                   */
/* Keep it short: collect input, set flags, never parse.     */
/* ────────────────────────────────────────────────────────── */
void on_uart_rx(void)
{
    while (uart_is_readable(UART_ID))
    {
        int ch = uart_getc(UART_ID);

        /* If main() has not consumed the previous command yet,
         * ignore new input until it does. */
        if (uart_command_received)
        {
            continue;
        }

        switch (ch)
        {
            /* Enter key: terminate buffer and signal main loop */
            case '\r':
            case '\n':
                uart_command_buffer[uart_command_index] = '\0';
                uart_command_received = true;
                uart_command_index = 0;
                break;

            /* Backspace / DEL */
            case '\b':
            case 127:
                if (uart_command_index > 0)
                {
                    uart_command_index--;
                    uart_command_buffer[uart_command_index] = '\0';

                    /* Move back, erase char, move back again */
                    uart_putc(UART_ID, '\b');
                    uart_putc(UART_ID, ' ');
                    uart_putc(UART_ID, '\b');
                }
                break;

            /* Allow only alphanumeric chars and spaces */
            default:
                if ((isalnum((unsigned char)ch) || ch == ' ') &&
                    uart_command_index < UART_BUF_LEN - 1)
                {
                    uart_command_buffer[uart_command_index] = (char)ch;
                    uart_command_index++;
                    uart_command_buffer[uart_command_index] = '\0';

                    uart_putc(UART_ID, (char)ch);   /* local echo */
                }
                /* Everything else is ignored */
                break;
        }
    }
}

/* ────────────────────────────────────────────────────────── */
/* Initialise UART0 and attach RX ISR                        */
/* ────────────────────────────────────────────────────────── */
void init_uart(void)
{
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART_ID, true, false);   /* RX on, TX off */
}

/* ────────────────────────────────────────────────────────── */
/* Move cursor to the end of current typed input             */
/* ────────────────────────────────────────────────────────── */
void display_cursor(void)
{
    term_move_to(INPUT_COL + uart_command_index, INPUT_ROW);
}

/* ────────────────────────────────────────────────────────── */
/* Clear only the typed-input area                           */
/* ────────────────────────────────────────────────────────── */
void clear_input(void)
{
    term_move_to(INPUT_COL, INPUT_ROW);
    term_erase_line();
}

/* ────────────────────────────────────────────────────────── */
/* Show last complete command near the top of the screen     */
/* Useful for testing and demos                              */
/* ────────────────────────────────────────────────────────── */
void display_command(const char *command)
{
    term_move_to(COMMAND_COL, COMMAND_ROW);
    term_erase_line();
    term_move_to(COMMAND_COL, COMMAND_ROW);
    printf("Got [%s]", command);
}

/* ────────────────────────────────────────────────────────── */
/* Draw static screen elements                               */
/* ────────────────────────────────────────────────────────── */
void display_background(void)
{
    term_set_color(clrWhite, clrBlack);
    term_cls();

    term_move_to(COMMAND_COL, COMMAND_ROW);
    printf("Got []");

    display_rgb_labels();
    display_rgb_values(red, green, blue);

    display_push_labels();
    display_push_values(push_states);

    display_adc_labels();

    term_move_to(VALUE_COL, LIGHT_ROW);
    printf("%4u", light_adc_value);

    term_move_to(VALUE_COL, TEMP_ROW);
    printf("%4u", temp_adc_value);

    term_move_to(COMMAND_COL, INPUT_ROW);
    printf("Next command: ");

    display_cursor();
}

/* ────────────────────────────────────────────────────────── */
/* Initialise RGB LED PWM                                    */
/* RGB LED is the one built into dev board              */
/* ────────────────────────────────────────────────────────── */
void init_rgb_led(void)
{
    uint slice;

    gpio_set_function(RED_PIN, GPIO_FUNC_PWM);
    gpio_set_function(GREEN_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BLUE_PIN, GPIO_FUNC_PWM);

    slice = pwm_gpio_to_slice_num(RED_PIN);
    pwm_set_wrap(slice, 65535);
    pwm_set_enabled(slice, true);

    slice = pwm_gpio_to_slice_num(GREEN_PIN);
    pwm_set_wrap(slice, 65535);
    pwm_set_enabled(slice, true);

    slice = pwm_gpio_to_slice_num(BLUE_PIN);
    pwm_set_wrap(slice, 65535);
    pwm_set_enabled(slice, true);

    set_rgb_led(0, 0, 0);
}

/* ────────────────────────────────────────────────────────── */
/* Set RGB brightness                                        */
/* Squares are used as required by the lab sheet             */
/* 0..255 becomes 0..65025, which fits under 65535           */
/* ────────────────────────────────────────────────────────── */
void set_rgb_led(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t r_level = (uint16_t)r * (uint16_t)r;
    uint16_t g_level = (uint16_t)g * (uint16_t)g;
    uint16_t b_level = (uint16_t)b * (uint16_t)b;

    pwm_set_gpio_level(RED_PIN, r_level);
    pwm_set_gpio_level(GREEN_PIN, g_level);
    pwm_set_gpio_level(BLUE_PIN, b_level);
}

/* ────────────────────────────────────────────────────────── */
/* Draw RGB labels                                           */
/* ────────────────────────────────────────────────────────── */
void display_rgb_labels(void)
{
    term_move_to(LABEL_COL, RED_ROW);
    printf("Red  :");

    term_move_to(LABEL_COL, GREEN_ROW);
    printf("Green:");

    term_move_to(LABEL_COL, BLUE_ROW);
    printf("Blue :");
}

/* ────────────────────────────────────────────────────────── */
/* Draw RGB values                                           */
/* %3u overwrites previous 0..255 values cleanly             */
/* ────────────────────────────────────────────────────────── */
void display_rgb_values(uint8_t r, uint8_t g, uint8_t b)
{
    term_move_to(VALUE_COL, RED_ROW);
    printf("%3u", r);

    term_move_to(VALUE_COL, GREEN_ROW);
    printf("%3u", g);

    term_move_to(VALUE_COL, BLUE_ROW);
    printf("%3u", b);
}

/* ────────────────────────────────────────────────────────── */
/* Process one complete command                              */
/* Accepted commands:
 *   red N
 *   green N
 *   blue N
 *   off
 *
 * A lowercase copy is used so Red / RED / Off also work.
 * ────────────────────────────────────────────────────────── */
bool process_command(const char *command)
{
    char lower[UART_BUF_LEN];
    int value = 0;
    int i;

    for (i = 0; i < UART_BUF_LEN - 1 && command[i] != '\0'; i++)
    {
        lower[i] = (char)tolower((unsigned char)command[i]);
    }
    lower[i] = '\0';

    if (strcmp(lower, "off") == 0)
    {
        red = 0;
        green = 0;
        blue = 0;
        return true;
    }

    if (sscanf(lower, "red %d", &value) == 1)
    {
        if (value >= 0 && value <= 255)
        {
            red = (uint8_t)value;
            return true;
        }
        return false;
    }

    if (sscanf(lower, "green %d", &value) == 1)
    {
        if (value >= 0 && value <= 255)
        {
            green = (uint8_t)value;
            return true;
        }
        return false;
    }

    if (sscanf(lower, "blue %d", &value) == 1)
    {
        if (value >= 0 && value <= 255)
        {
            blue = (uint8_t)value;
            return true;
        }
        return false;
    }

    return false;
}

/* ────────────────────────────────────────────────────────── */
/* Initialise pushbutton GPIOs and one shared callback       */
/* The schematic shows external pull-ups, so buttons are     */
/* active-low:
 *   pressed  -> low
 *   released -> high
 * ────────────────────────────────────────────────────────── */
void init_push_buttons(void)
{
    for (int i = 0; i < PUSH_PIN_COUNT; i++)
    {
        gpio_init(push_pins[i]);
        gpio_set_dir(push_pins[i], GPIO_IN);

        gpio_set_irq_enabled_with_callback(
            push_pins[i],
            GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
            true,
            &on_gpio_change
        );

        /* Initial state: high means button is up */
        push_states[i] = gpio_get(push_pins[i]);
    }

    push_state_changed = true;
}

/* ────────────────────────────────────────────────────────── */
/* Shared GPIO callback for all three pushbuttons            */
/* fall -> DOWN, rise -> UP                                  */
/* ────────────────────────────────────────────────────────── */
void on_gpio_change(uint gpio, uint32_t events)
{
    for (int i = 0; i < PUSH_PIN_COUNT; i++)
    {
        if (gpio == (uint)push_pins[i])
        {
            if (events & GPIO_IRQ_EDGE_FALL)
            {
                push_states[i] = false;
            }
            else if (events & GPIO_IRQ_EDGE_RISE)
            {
                push_states[i] = true;
            }

            push_state_changed = true;
            break;
        }
    }
}

/* ────────────────────────────────────────────────────────── */
/* Draw pushbutton labels                                    */
/* ────────────────────────────────────────────────────────── */
void display_push_labels(void)
{
    term_move_to(LABEL_COL, PUSH_ROW_TOP + 0);
    printf("Push1:");

    term_move_to(LABEL_COL, PUSH_ROW_TOP + 1);
    printf("Push2:");

    term_move_to(LABEL_COL, PUSH_ROW_TOP + 2);
    printf("Push3:");
}

/* ────────────────────────────────────────────────────────── */
/* Draw pushbutton states                                    */
/* %-5s ensures UP fully overwrites DOWN and vice versa      */
/* ────────────────────────────────────────────────────────── */
void display_push_values(const volatile bool *states)
{
    for (int i = 0; i < PUSH_PIN_COUNT; i++)
    {
        term_move_to(VALUE_COL, PUSH_ROW_TOP + i);
        printf("%-5s", states[i] ? "UP" : "DOWN");
    }
}

/* ────────────────────────────────────────────────────────── */
/* Initialise ADC                                            */
/* LDR uses GPIO26 / ADC0
 * Temp sensor uses internal ADC4
 * ────────────────────────────────────────────────────────── */
void init_adc(void)
{
    adc_init();
    adc_gpio_init(LDR_ADC_PIN);
    adc_set_temp_sensor_enabled(true);
}

/* ────────────────────────────────────────────────────────── */
/* Read both ADC sources                                     */
/* Return true if either value changed                       */
/* ────────────────────────────────────────────────────────── */
bool read_adc_values(void)
{
    uint16_t new_light;
    uint16_t new_temp;
    bool changed = false;

    adc_select_input(LDR_ADC_CHANNEL);
    new_light = adc_read();

    adc_select_input(TEMP_ADC_CHANNEL);
    new_temp = adc_read();

    if (new_light != light_adc_value)
    {
        light_adc_value = new_light;
        changed = true;
    }

    if (new_temp != temp_adc_value)
    {
        temp_adc_value = new_temp;
        changed = true;
    }

    return changed;
}

/* ────────────────────────────────────────────────────────── */
/* Draw ADC labels                                           */
/* ────────────────────────────────────────────────────────── */
void display_adc_labels(void)
{
    term_move_to(LABEL_COL, LIGHT_ROW);
    printf("Light:");

    term_move_to(LABEL_COL, TEMP_ROW);
    printf("Temp :");
}

/* ────────────────────────────────────────────────────────── */
/* Update ADC values on screen if they changed               */
/* ────────────────────────────────────────────────────────── */
void display_adc_values(void)
{
    if (read_adc_values())
    {
        term_move_to(VALUE_COL, LIGHT_ROW);
        printf("%4u", light_adc_value);

        term_move_to(VALUE_COL, TEMP_ROW);
        printf("%4u", temp_adc_value);
    }
}

/* ────────────────────────────────────────────────────────── */
/* Initialise repeating timer                                */
/* ────────────────────────────────────────────────────────── */
void init_timer(void)
{
    add_repeating_timer_ms(TIMER_INTERVAL_MS, on_timer, NULL, &timer);
}

/* ────────────────────────────────────────────────────────── */
/* Timer callback                                            */
/* Keep it tiny: just set a flag                             */
/* ────────────────────────────────────────────────────────── */
bool on_timer(struct repeating_timer *t)
{
    (void)t;
    timer_ticked = true;
    return true;
}

/* ────────────────────────────────────────────────────────── */
/* main                                                      */
/* Event loop:
 *   - handle completed UART commands
 *   - handle button state changes
 *   - handle timer ticks / ADC refresh
 *   - sleep until next interrupt
 * ────────────────────────────────────────────────────────── */
int main(void)
{
    stdio_init_all();

    init_uart();
    init_rgb_led();
    init_push_buttons();
    init_adc();
    init_timer();

    /* Read initial ADC values before drawing the screen */
    read_adc_values();
    display_background();

    while (true)
    {
        if (uart_command_received)
        {
            display_command((const char *)uart_command_buffer);

            if (process_command((const char *)uart_command_buffer))
            {
                set_rgb_led(red, green, blue);
                display_rgb_values(red, green, blue);
            }

            clear_input();
            display_cursor();

            uart_command_received = false;
        }

        if (push_state_changed)
        {
            display_push_values(push_states);
            display_cursor();
            push_state_changed = false;
        }

        if (timer_ticked)
        {
            display_adc_values();
            display_cursor();
            timer_ticked = false;
        }

        /* Sleep until the next interrupt/event */
        __asm volatile("wfi");
    }
}