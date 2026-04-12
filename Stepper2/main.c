/**************************************************************
 * main.c
 * CC2511 Lab 5 - Advanced control using text commands
 *
 * This program:
 * 1. Collects text commands from the terminal into a buffer
 * 2. Handles backspace and Enter correctly
 * 3. Parses complete commands
 * 4. Controls a stepper motor through a DRV8825 driver
 *
 * Supported commands:
 *   fwd <steps>
 *   back <steps>
 *   delay high <time_us>
 *   delay low <time_us>
 *   mode <1|2|4|8|16|32>
 * ***********************************************************/

#include "pico/stdlib.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 32          // change only this to safely resize the buffer

volatile char command_buffer[BUFFER_SIZE];  // stores typed command
volatile unsigned int buffer_index = 0;    // next empty slot in buffer
volatile bool flag_complete = false;       // true when Enter finishes a command

// DRV8825 control pins
#define STEP_PIN 14
#define DIR_PIN  15
#define M0_PIN   18
#define M1_PIN   19
#define M2_PIN   20

int high_delay    = 1000;   // STEP high time in us
int low_delay     = 1000;   // STEP low time in us
int microstepping = 1;      // current microstep mode

/*
Start with high_delay = 1000, low_delay = 1000 — motor works but slowly
Send "delay high 2" via terminal → now high is correct
Send "delay low 500" → test if motor still works
Keep lowering low_delay until steps start missing, then back off
*/

// Set up all GPIO pins used by the driver
void init_pins(void)
{
    gpio_init(STEP_PIN);
    gpio_init(DIR_PIN);
    gpio_init(M0_PIN);
    gpio_init(M1_PIN);
    gpio_init(M2_PIN);

    gpio_set_dir(STEP_PIN, GPIO_OUT);
    gpio_set_dir(DIR_PIN,  GPIO_OUT);
    gpio_set_dir(M0_PIN,   GPIO_OUT);
    gpio_set_dir(M1_PIN,   GPIO_OUT);
    gpio_set_dir(M2_PIN,   GPIO_OUT);

    // Start with all outputs low
    gpio_put(STEP_PIN, 0);
    gpio_put(DIR_PIN,  0);
    gpio_put(M0_PIN,   0);
    gpio_put(M1_PIN,   0);
    gpio_put(M2_PIN,   0);
}

// Send one STEP pulse (used by step_motor)
void send_one_pulse(void)
{
    gpio_put(STEP_PIN, 1);      // pulse high
    sleep_us(high_delay);
    gpio_put(STEP_PIN, 0);      // pulse low
    sleep_us(low_delay);
}

// Step the motor once in the specified direction
void step_motor(bool fwd)
{
    gpio_put(DIR_PIN, fwd);     // set direction
    sleep_us(10);               // let DIR settle before stepping
    send_one_pulse();
}

// Set MODE pins for DRV8825 microstepping
// MODE2:MODE1:MODE0 encodes n where STEP = 2^n
void set_microstepping(int microsteps)
{
    switch (microsteps)
    {
    case 1:   gpio_put(M0_PIN, 0); gpio_put(M1_PIN, 0); gpio_put(M2_PIN, 0); break; // 000
    case 2:   gpio_put(M0_PIN, 1); gpio_put(M1_PIN, 0); gpio_put(M2_PIN, 0); break; // 001
    case 4:   gpio_put(M0_PIN, 0); gpio_put(M1_PIN, 1); gpio_put(M2_PIN, 0); break; // 010
    case 8:   gpio_put(M0_PIN, 1); gpio_put(M1_PIN, 1); gpio_put(M2_PIN, 0); break; // 011
    case 16:  gpio_put(M0_PIN, 0); gpio_put(M1_PIN, 0); gpio_put(M2_PIN, 1); break; // 100
    case 32:  gpio_put(M0_PIN, 1); gpio_put(M1_PIN, 0); gpio_put(M2_PIN, 1); break; // 101
    default:  return;   // ignore invalid values
    }

    microstepping = microsteps;
    sleep_us(10);               // let MODE pins settle
}

// Read terminal input without blocking.
// Uses switch (as shown in lecture) to handle \r/\n, backspace, and normal chars.
void process_input(void)
{
    int ch = getchar_timeout_us(0);     // returns PICO_ERROR_TIMEOUT if nothing waiting

    while (ch != PICO_ERROR_TIMEOUT)
    {
        if (!flag_complete)             // ignore new input until current command is processed
        {
            switch (ch)
            {
            // Enter pressed: null-terminate and mark command ready
            case '\r': case '\n':
                if (buffer_index > 0)
                {
                    command_buffer[buffer_index] = '\0'; // makes it a proper C string
                    flag_complete = true;
                    printf("\r\n");
                }
                break;

            // Backspace: remove last character from buffer and terminal
            case '\b': case 127:
                if (buffer_index > 0)
                {
                    buffer_index--;         // one step back in buffer
                    printf("\b \b");        // erase character on terminal
                }
                break;

            // Normal character: store if there is room (leave 1 byte for '\0')
            default:
                if (buffer_index < BUFFER_SIZE - 1)
                {
                    command_buffer[buffer_index] = (char)ch;
                    buffer_index++;
                    putchar(ch);            // echo back so user sees what they typed
                }
                break;
            }
        }

        ch = getchar_timeout_us(0);     // check for next available character
    }
}

// Parse one null-terminated command and execute it.
// Writes a result or error message to report.
// Returns true on success, false on failure.
bool process_command(const char *buffer, char *report, int report_buf_size)
{
    int  steps, time_us, mode_value;
    char type[8];   // "high" or "low" - 4 chars + '\0', %7s limits to 7 so 8 bytes is safe
    char extra;     // catches any unexpected extra input after a command
    int  n;         // sscanf return value - number of items matched

    // ---- fwd <steps> ----
    n = sscanf(buffer, "fwd %d %c", &steps, &extra);
    if (n == 1)     // exactly steps matched, nothing extra
    {
        if (steps < 0 || steps > 2000)
        {
            snprintf(report, report_buf_size,
                     "steps parameter out of range: expected 0-2000, got %d", steps);
            return false;
        }
        for (int i = 0; i < steps; i++) { step_motor(true); }
        snprintf(report, report_buf_size, "fwd: moved by %d steps", steps);
        return true;
    }
    if (n == 2) { snprintf(report, report_buf_size, "invalid command"); return false; }

    // ---- back <steps> ----
    n = sscanf(buffer, "back %d %c", &steps, &extra);
    if (n == 1)
    {
        if (steps < 0 || steps > 2000)
        {
            snprintf(report, report_buf_size,
                     "steps parameter out of range: expected 0-2000, got %d", steps);
            return false;
        }
        for (int i = 0; i < steps; i++) { step_motor(false); }
        snprintf(report, report_buf_size, "back: moved by %d steps", steps);
        return true;
    }
    if (n == 2) { snprintf(report, report_buf_size, "invalid command"); return false; }

    // ---- delay <high|low> <time_us> ----
    n = sscanf(buffer, "delay %7s %d %c", type, &time_us, &extra);
    if (n == 2)     // type and time matched, nothing extra
    {
        if (strcmp(type, "high") != 0 && strcmp(type, "low") != 0)
        {
            snprintf(report, report_buf_size,
                     "type parameter out of range: expected high or low, got %s", type);
            return false;
        }
        if (time_us < 0 || time_us > 1000000)
        {
            snprintf(report, report_buf_size,
                     "delay parameter out of range: expected 0-1000000, got %d", time_us);
            return false;
        }
        if (strcmp(type, "high") == 0)
        {
            high_delay = time_us;
            snprintf(report, report_buf_size, "high delay set to %d us", time_us);
        }
        else
        {
            low_delay = time_us;
            snprintf(report, report_buf_size, "low delay set to %d us", time_us);
        }
        return true;
    }
    if (n == 3) { snprintf(report, report_buf_size, "invalid command"); return false; }

    // ---- mode <1|2|4|8|16|32> ----
    n = sscanf(buffer, "mode %d %c", &mode_value, &extra);
    if (n == 1)
    {
        if (!(mode_value == 1  || mode_value == 2  || mode_value == 4 ||
              mode_value == 8  || mode_value == 16 || mode_value == 32))
        {
            snprintf(report, report_buf_size,
                     "microsteps parameter out of range: expected 1,2,4,8,16 or 32, got %d",
                     mode_value);
            return false;
        }
        set_microstepping(mode_value);
        snprintf(report, report_buf_size, "microsteps set to %d", mode_value);
        return true;
    }
    if (n == 2) { snprintf(report, report_buf_size, "invalid command"); return false; }

    // No command matched
    snprintf(report, report_buf_size, "invalid command");
    return false;
}

int main(void)
{
    stdio_init_all();       // enable terminal I/O
    init_pins();            // configure motor GPIO pins

    char report_buf[100];   // receives success or error message from process_command

    printf("Stepper Motor Control Initialized\r\n");

    while (true)
    {
        process_input();    // collect typed characters (non-blocking)

        if (flag_complete)  // only parse once a full command has arrived
        {
            process_command((const char *)command_buffer, report_buf, sizeof(report_buf));
            printf("%s\r\n", report_buf);

            // Reset for next command
            buffer_index = 0;
            command_buffer[0] = '\0';
            flag_complete = false;
        }
    }
}

/* ---- Things to remember ----
 * buffer_index        = next empty slot (not last filled)
 * flag_complete       = Enter was pressed; command is ready to parse
 * '\0'                = null terminator; turns char array into a C string
 * getchar_timeout_us(0) = non-blocking read; returns PICO_ERROR_TIMEOUT if empty
 * sscanf return value = number of items successfully matched
 * %c at end of sscanf format = catches any extra junk after expected args
 * %7s in sscanf      = width limit; buffer must be 8 bytes (7 + '\0')
 * switch on ch       = lecture-pattern for handling \r/\n, backspace, default
 * snprintf           = safe printf into a buffer (won't overflow)
 */