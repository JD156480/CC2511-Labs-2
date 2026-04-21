/**************************************************************
 * main.c
 * rev 1.3 13-Apr-2026
 * DevBoardLab – Week 7/8 Lab, Parts 1–6
 *
 * Event-driven terminal UI for the CC2511 Dev Board.
 *
 * Design pattern:
 *   ISR/callback -> capture event, set flag, do minimal work
 *   main loop    -> process flags, update display/hardware, sleep
 *
 * Parts 1–5 implement the required lab behaviour.
 * Part 6 adds optional terminal panels and help text.
 * create new prog: python "%pico_repos_path%\ppgen\ppgen.py" programnamehere -l pwm adc uart -d picoprobe2
 **************************************************************/

#include <stdio.h>      // printf()
#include <stdbool.h>    // bool, true, false
#include <stdint.h>     // fixed-width types like uint8_t, uint16_t
#include <string.h>     // string functions like strcmp(), strlen()
#include <ctype.h>      // character checks like isalnum(), tolower()

#include "pico/stdlib.h"    // basic Pico SDK functions: stdio, sleep, GPIO basics
#include "pico/time.h"      // time functions: get_absolute_time(), time differences

#include "hardware/uart.h"  // low-level UART functions and settings
#include "hardware/irq.h"   // interrupt setup and handlers
#include "hardware/gpio.h"  // GPIO control: input/output pins, reads, writes
#include "hardware/pwm.h"   // PWM control for LED brightness etc.
#include "hardware/adc.h"   // ADC functions for reading analog values

#include "terminal.h"       // terminal cursor movement, colours, screen layout

/* -- UART config ------------------------------------------- */
#define UART_ID      uart0
#define BAUD_RATE    115200
#define UART_TX_PIN  0
#define UART_RX_PIN  1
#define UART_BUF_LEN 32   /* max command + null terminator    */

/* -- Dev board pin numbers --------------------------------- */
#define RED_PIN          11
#define GREEN_PIN        12
#define BLUE_PIN         13

#define PUSH_PIN_COUNT   3
#define PUSH1_PIN        2
#define PUSH2_PIN        3
#define PUSH3_PIN        4

#define LDR_ADC_PIN      26
#define LDR_ADC_CHANNEL  0
#define TEMP_ADC_CHANNEL 4   /* internal temperature sensor   */

/* -- Timer ------------------------------------------------- */
#define TIMER_INTERVAL_MS 100   /* 10 Hz refresh rate         */

/* -- Screen layout (Part 6 panel design) ------------------- */
#define OUTER_LEFT    1
#define OUTER_TOP     1
#define OUTER_WIDTH   60
#define OUTER_HEIGHT  18

#define RGB_LEFT      2
#define RGB_TOP       4
#define RGB_WIDTH     18
#define RGB_HEIGHT    5

#define PUSH_LEFT     22
#define PUSH_TOP      4
#define PUSH_WIDTH    18
#define PUSH_HEIGHT   5

#define ADC_LEFT      2
#define ADC_TOP       10
#define ADC_WIDTH     18
#define ADC_HEIGHT    4

#define HELP_LEFT     22
#define HELP_TOP      10
#define HELP_WIDTH    34
#define HELP_HEIGHT   7

/* -- Fixed element positions ------------------------------- */
#define COMMAND_COL     3
#define COMMAND_ROW     2
#define STATUS_COL      3
#define STATUS_ROW      3

#define RGB_LABEL_COL   4
#define RGB_VALUE_COL   12
#define RED_ROW         5
#define GREEN_ROW       6
#define BLUE_ROW        7

#define PUSH_LABEL_COL  24
#define PUSH_VALUE_COL  32
#define PUSH_ROW_TOP    5

#define ADC_LABEL_COL   4
#define ADC_VALUE_COL   12
#define LIGHT_ROW       11
#define TEMP_ROW        12

#define HELP_TEXT_COL   24
#define HELP_TEXT_ROW   11

#define INPUT_LABEL_COL 24
#define INPUT_ROW       15
#define INPUT_COL       38

/* Width of typed-text area, derived from panel geometry */
#define INPUT_WIDTH  (HELP_LEFT + HELP_WIDTH - 2 - INPUT_COL)

/* -- Shared state: volatile because modified by ISR/callbacks */
volatile bool uart_command_received  = false;
volatile char uart_command_buffer[UART_BUF_LEN];
volatile int  uart_command_index     = 0;

/* true = UP (pin high), false = DOWN (pin low) */
int           push_pins[PUSH_PIN_COUNT]    = {PUSH1_PIN, PUSH2_PIN, PUSH3_PIN};
volatile bool push_states[PUSH_PIN_COUNT]  = {true, true, true};
volatile bool push_state_changed           = true;

struct repeating_timer timer;
volatile bool timer_ticked = true;

/* -- Normal (non-ISR) program state ------------------------ */
uint8_t  red = 0, green = 0, blue = 0;
uint16_t light_adc_value = 0;
uint16_t temp_adc_value  = 0;

/* -- Forward declarations ----------------------------------- */
void display_command(const char *command);
void display_status(const char *message);
void on_uart_rx(void);
void init_uart(void);
void display_cursor(void);
void clear_input(void);
void display_background(void);

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

static void draw_box(int left, int top, int width, int height,
                     const char *title, unsigned short background);
static void draw_multiline_text(int left, int top, const char *text);

/* ============================= */
/* Part 1 – UART interrupt handling                          */
/* ============================= */

/* UART RX ISR.
 * Keep this short: collect input only, do not parse here.
 */
void on_uart_rx(void)
{
    while (uart_is_readable(UART_ID))
    {
        uint8_t ch = uart_getc(UART_ID);

        /* If main() has not consumed the previous command yet, discard */
        if (uart_command_received)
        {
            continue;
        }

        switch (ch)
        {
            /* Enter: terminate string and signal main loop */
            case '\r':
            case '\n':
                uart_command_buffer[uart_command_index] = '\0';
                uart_command_received = true;
                uart_command_index = 0;
                break;

            /* Backspace / DEL: remove one character */
            case '\b':
            case 127:
                if (uart_command_index > 0)
                {
                    uart_command_index--;
                    uart_command_buffer[uart_command_index] = '\0';

                    /* visual erase: back, blank, back */
                    uart_putc(UART_ID, '\b');
                    uart_putc(UART_ID, ' ');
                    uart_putc(UART_ID, '\b');
                }
                break;

            /* Accept letters, digits and spaces only */
            default:
                if ((isalnum((unsigned char)ch) || ch == ' ') &&
                    uart_command_index < UART_BUF_LEN - 1)
                {
                    uart_command_buffer[uart_command_index++] = ch;
                    uart_command_buffer[uart_command_index] = '\0';
                    uart_putc(UART_ID, ch);   /* local echo */
                }
                break;
        }
    }
}

/* Set up UART0 and attach RX ISR */
void init_uart(void)
{
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART_ID, true, false);   /* RX on, TX off */
}

/* ============================= */
/* Part 2 – Terminal management                              */
/* ============================= */

/* Move cursor to end of current typed input */
void display_cursor(void)
{
    int idx = uart_command_index < INPUT_WIDTH
              ? uart_command_index : INPUT_WIDTH;
    term_move_to(INPUT_COL + idx, INPUT_ROW);
}

/* Clear only the input area 
- move to row
- blank out old text
- move back
- print new text*/
void clear_input(void)
{
    term_move_to(INPUT_COL, INPUT_ROW);
    for (int i = 0; i < INPUT_WIDTH; i++)
    {
        printf(" ");
    }
}

/* Show most recently received command */
void display_command(const char *command)
{
    /* Clear only the command area inside the outer box 
    and again - could use helper, but eh. 
    quicker to recognise for exam.*/
    term_move_to(COMMAND_COL, COMMAND_ROW);
    for (int i = 0; i < OUTER_WIDTH - COMMAND_COL - 1; i++)
    {
        printf(" ");
    }

    term_move_to(COMMAND_COL, COMMAND_ROW);
    printf("Got [%s]", command);
}

/* Show short status / feedback message */
void display_status(const char *message)
{
    /* Clear only the status area inside the outer box */
    term_move_to(STATUS_COL, STATUS_ROW);
    for (int i = 0; i < OUTER_WIDTH - STATUS_COL - 1; i++)
    {
        printf(" ");
    }

    term_move_to(STATUS_COL, STATUS_ROW);
    printf("%s", message);
}

/* Draw all fixed/static screen elements */
void display_background(void)
{
    term_set_color(clrWhite, clrBlack);
    term_cls();
    term_move_to(1, 1);

    /* Part 6 panels */
    draw_box(OUTER_LEFT, OUTER_TOP, OUTER_WIDTH, OUTER_HEIGHT,
             "CC2511 Dev Board Monitor", clrBlue);
    draw_box(RGB_LEFT,  RGB_TOP,  RGB_WIDTH,  RGB_HEIGHT,  "RGB LED",      clrMagenta);
    draw_box(PUSH_LEFT, PUSH_TOP, PUSH_WIDTH, PUSH_HEIGHT, "Push Buttons", clrCyan);
    draw_box(ADC_LEFT,  ADC_TOP,  ADC_WIDTH,  ADC_HEIGHT,  "ADC Readings", clrGreen);
    draw_box(HELP_LEFT, HELP_TOP, HELP_WIDTH, HELP_HEIGHT, "Input / Help", clrBlue);

    display_command("");
    display_status("");

    display_rgb_labels();
    display_rgb_values(red, green, blue);

    display_push_labels();
    display_push_values(push_states);

    display_adc_labels();
    term_move_to(ADC_VALUE_COL, LIGHT_ROW); printf("%4u", light_adc_value);
    term_move_to(ADC_VALUE_COL, TEMP_ROW);  printf("%4u", temp_adc_value);

    draw_multiline_text(HELP_TEXT_COL, HELP_TEXT_ROW,
        "Commands:\n"
        " red N\n"
        " green N\n"
        " blue N\n"
        " off");

    term_move_to(INPUT_LABEL_COL, INPUT_ROW);
    printf("Next command: ");

    display_cursor();
}

/* ============================= */
/* Part 3 – RGB LED and command processor                    */
/* ============================= */

/* Initialise PWM for all three LED pins */
void init_rgb_led(void)
{
    uint slice;
    const int pins[3] = {RED_PIN, GREEN_PIN, BLUE_PIN};

    for (int i = 0; i < 3; i++)
    {
        gpio_set_function(pins[i], GPIO_FUNC_PWM);
        slice = pwm_gpio_to_slice_num(pins[i]);
        pwm_set_wrap(slice, 65535);
        pwm_set_enabled(slice, true);
    }

    set_rgb_led(0, 0, 0);
}

/* Set brightness using squared 0..255 values */
void set_rgb_led(uint8_t r, uint8_t g, uint8_t b)
{
    pwm_set_gpio_level(RED_PIN,   (uint16_t)r * r);
    pwm_set_gpio_level(GREEN_PIN, (uint16_t)g * g);
    pwm_set_gpio_level(BLUE_PIN,  (uint16_t)b * b);
}

void display_rgb_labels(void)
{
    term_move_to(RGB_LABEL_COL, RED_ROW);   printf("Red  :");
    term_move_to(RGB_LABEL_COL, GREEN_ROW); printf("Green:");
    term_move_to(RGB_LABEL_COL, BLUE_ROW);  printf("Blue :");
}

/* %3u overwrites old values cleanly */
void display_rgb_values(uint8_t r, uint8_t g, uint8_t b)
{
    term_move_to(RGB_VALUE_COL, RED_ROW);   printf("%3u", r);
    term_move_to(RGB_VALUE_COL, GREEN_ROW); printf("%3u", g);
    term_move_to(RGB_VALUE_COL, BLUE_ROW);  printf("%3u", b);
}

/* Parse one command and update RGB globals
 * Case-insensitive: Red / RED / red all work.
 */
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
        red = green = blue = 0;
        return true;
    }
    if (sscanf(lower, "red %d", &value) == 1)
    {
        if (value < 0 || value > 255) return false;
        red = (uint8_t)value;
        return true;
    }
    if (sscanf(lower, "green %d", &value) == 1)
    {
        if (value < 0 || value > 255) return false;
        green = (uint8_t)value;
        return true;
    }
    if (sscanf(lower, "blue %d", &value) == 1)
    {
        if (value < 0 || value > 255) return false;
        blue = (uint8_t)value;
        return true;
    }

    return false;
}

/* ============================= */
/* Part 4 – Pushbutton interrupt handler                     */
/* ============================= */

/* Initialise all pushbutton pins and then enable interrupts.
 * Buttons are active-low:
 *   high = UP
 *   low  = DOWN
 */
void init_push_buttons(void)
{
    /* First configure all pins properly */
    for (int i = 0; i < PUSH_PIN_COUNT; i++)
    {
        gpio_init(push_pins[i]);
        gpio_set_dir(push_pins[i], GPIO_IN);
        gpio_pull_up(push_pins[i]);
        push_states[i] = gpio_get(push_pins[i]);   /* capture initial state */
    }

    /* Then attach callback and enable interrupts */
    gpio_set_irq_enabled_with_callback(
        push_pins[0],
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
        true,
        &on_gpio_change);

    for (int i = 1; i < PUSH_PIN_COUNT; i++)
    {
        gpio_set_irq_enabled(push_pins[i],
                             GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
                             true);
    }

    push_state_changed = true;
}

/* Shared callback for all three buttons */
void on_gpio_change(uint gpio, uint32_t events)
{
    for (int i = 0; i < PUSH_PIN_COUNT; i++)
    {
        if (gpio == (uint)push_pins[i])
        {
            push_states[i] = (events & GPIO_IRQ_EDGE_RISE) ? true : false;
            push_state_changed = true;
            break;
        }
    }
}

void display_push_labels(void)
{
    term_move_to(PUSH_LABEL_COL, PUSH_ROW_TOP + 0); printf("Push1:");
    term_move_to(PUSH_LABEL_COL, PUSH_ROW_TOP + 1); printf("Push2:");
    term_move_to(PUSH_LABEL_COL, PUSH_ROW_TOP + 2); printf("Push3:");
}

/* %-5s ensures old text is fully overwritten */
void display_push_values(const volatile bool *states)
{
    for (int i = 0; i < PUSH_PIN_COUNT; i++)
    {
        term_move_to(PUSH_VALUE_COL, PUSH_ROW_TOP + i);
        printf("%-5s", states[i] ? "UP" : "DOWN");
    }
}

/* ============================= */
/* Part 5 – Repeating timer and ADC                          */
/* ============================= */

void init_timer(void)
{
    add_repeating_timer_ms(TIMER_INTERVAL_MS, on_timer, NULL, &timer);
}

/* Timer callback: set flag only */
bool on_timer(struct repeating_timer *t)
{
    (void)t;
    timer_ticked = true;
    return true;   /* keep repeating */
}

/* ADC0 = LDR, ADC4 = internal temperature */
void init_adc(void)
{
    adc_init();
    adc_gpio_init(LDR_ADC_PIN);
    adc_set_temp_sensor_enabled(true);
}

/* Read both ADC channels into globals */
bool read_adc_values(void)
{
    bool changed = false;
    uint16_t val;

    adc_select_input(LDR_ADC_CHANNEL);
    val = adc_read();
    if (val != light_adc_value)
    {
        light_adc_value = val;
        changed = true;
    }

    adc_select_input(TEMP_ADC_CHANNEL);
    val = adc_read();
    if (val != temp_adc_value)
    {
        temp_adc_value = val;
        changed = true;
    }

    return changed;
}

void display_adc_labels(void)
{
    term_move_to(ADC_LABEL_COL, LIGHT_ROW); printf("Light:");
    term_move_to(ADC_LABEL_COL, TEMP_ROW);  printf("Temp :");
}

/* Update ADC display only if value changed */
void display_adc_values(void)
{
    if (read_adc_values())
    {
        term_move_to(ADC_VALUE_COL, LIGHT_ROW); printf("%4u", light_adc_value);
        term_move_to(ADC_VALUE_COL, TEMP_ROW);  printf("%4u", temp_adc_value);
    }
}

/* ============================= */
/* Part 6 – Terminal panel utilities (extension)             */
/* ============================= */

/* Draw coloured panel border with optional title */
static void draw_box(int left, int top, int width, int height,
                     const char *title, unsigned short background)
{
    term_set_color(clrWhite, background);

    /* Top and bottom edges */
    for (int pass = 0; pass < 2; pass++)
    {
        term_move_to(left, pass == 0 ? top : top + height - 1);
        for (int c = 0; c < width; c++) printf(" ");
    }

    /* Left/right edges + black interior */
    for (int row = 1; row < height - 1; row++)
    {
        term_set_color(clrWhite, background);
        term_move_to(left, top + row);             printf(" ");
        term_move_to(left + width - 1, top + row); printf(" ");

        term_set_color(clrWhite, clrBlack);
        term_move_to(left + 1, top + row);
        for (int c = 1; c < width - 1; c++) printf(" ");
    }

    /* Centred title */
    if (title != NULL && title[0] != '\0')
    {
        int tlen = (int)strlen(title);
        if (tlen < width - 4)
        {
            term_set_color(clrWhite, background);
            term_move_to(left + (width - tlen) / 2, top);
            printf("%s", title);
        }
    }

    term_set_color(clrWhite, clrBlack);
}

/* Print multiline text starting at left, top */
static void draw_multiline_text(int left, int top, const char *text)
{
    int x = left;
    int y = top;

    term_move_to(x, y);

    for (int i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == '\n')
        {
            y++;
            x = left;
            term_move_to(x, y);
        }
        else
        {
            printf("%c", text[i]);
            x++;
        }
    }
}

/* ============================= */
/* main – event loop                                         */
/* ============================= */
int main(void)
{
    stdio_init_all();

    init_uart();
    init_rgb_led();
    init_push_buttons();
    init_adc();
    init_timer();

    display_background();

    while (true)
    {
        /* -- UART command received ----------------------- */
        if (uart_command_received)
        {
            display_command((const char *)uart_command_buffer);

            if (process_command((const char *)uart_command_buffer))
            {
                set_rgb_led(red, green, blue);
                display_rgb_values(red, green, blue);
                display_status("OK");
            }
            else
            {
                display_status("Invalid command");
            }

            clear_input();
            display_cursor();
            uart_command_received = false;   /* release buffer to ISR */
        }

        /* -- Pushbutton state changed -------------------- */
        if (push_state_changed)
        {
            display_push_values(push_states);
            display_cursor();
            push_state_changed = false;
        }

        /* -- Timer tick: refresh ADC display ------------- */
        if (timer_ticked)
        {
            display_adc_values();
            display_cursor();
            timer_ticked = false;
        }

        /* Sleep until next interrupt (UART, GPIO, timer) */
        __asm volatile("wfi");
    }
}