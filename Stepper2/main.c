/**************************************************************
 * main.c
 * CC2511 Lab 5 - Stepper2
 * Advanced control using text commands
 *
 * This program:
 * 1. Reads terminal input into a text buffer
 * 2. Handles Enter and backspace correctly
 * 3. Detects when a full command has been entered
 * 4. Parses and validates commands
 * 5. Controls a DRV8825 stepper motor driver
 *
 * Supported commands:
 *   fwd <steps>
 *   back <steps>
 *   delay high <time_us>
 *   delay low <time_us>
 *   mode <1|2|4|8|16|32>
 *
 * Design idea:
 * - process_input() only deals with collecting characters
 * - process_command() only deals with parsing/executing commands
 * - main() repeatedly checks for input, then processes full commands
 ***************************************************************/

#include "pico/stdlib.h"   // Pico GPIO, timing, stdio support
#include <stdbool.h>       // bool, true, false
#include <stdio.h>         // printf, snprintf, sscanf, putchar
#include <string.h>        // strcmp
#include <ctype.h>         // isalnum

/* Size of terminal command buffer.
 * Change only this value if you want a larger/smaller buffer. */
#define BUFFER_SIZE 32

/* -------- Command buffer state --------
 * command_buffer = stores typed characters
 * buffer_index   = next empty slot in the buffer
 * flag_complete  = true once Enter has been pressed
 *
 * buffer_index is the NEXT free position, not the last used one.
 * Example:
 *   "abc"
 *    012
 * buffer_index will be 3
 */
char command_buffer[BUFFER_SIZE];
unsigned int buffer_index = 0;
bool flag_complete = false;

/* -------- DRV8825 control pins --------
 * Update these if your wiring is different. */
#define STEP_PIN 14
#define DIR_PIN  15
#define M0_PIN   18
#define M1_PIN   19
#define M2_PIN   20

/* -------- Step pulse timing --------
 * high_delay = STEP high time in microseconds
 * low_delay  = STEP low time in microseconds
 *
 * Start with safe/slower values, then tune using commands.
 */
int high_delay = 1000;
int low_delay  = 1000;

/* Stores current microstepping mode.
 * Not essential for movement, but useful to track current state. */
int microstepping = 1;

/* ============================================================
 * Initialise all GPIO pins used by the stepper motor driver
 * ============================================================ */
void init_pins(void)
{
    /* Prepare the GPIO pins */
    gpio_init(STEP_PIN);
    gpio_init(DIR_PIN);
    gpio_init(M0_PIN);
    gpio_init(M1_PIN);
    gpio_init(M2_PIN);

    /* Set all of them as outputs */
    gpio_set_dir(STEP_PIN, GPIO_OUT);
    gpio_set_dir(DIR_PIN,  GPIO_OUT);
    gpio_set_dir(M0_PIN,   GPIO_OUT);
    gpio_set_dir(M1_PIN,   GPIO_OUT);
    gpio_set_dir(M2_PIN,   GPIO_OUT);

    /* Start with outputs low */
    gpio_put(STEP_PIN, 0);
    gpio_put(DIR_PIN,  0);
    gpio_put(M0_PIN,   0);
    gpio_put(M1_PIN,   0);
    gpio_put(M2_PIN,   0);
}

/* ============================================================
 * Send one pulse to the STEP pin
 * ============================================================
 * DRV8825 advances one microstep/full-step on a STEP pulse.
 * Pulse shape:
 *   STEP high for high_delay microseconds
 *   STEP low  for low_delay  microseconds
 */
void send_one_pulse(void)
{
    gpio_put(STEP_PIN, 1);     // set STEP high
    sleep_us(high_delay);      // hold it high
    gpio_put(STEP_PIN, 0);     // set STEP low
    sleep_us(low_delay);       // hold it low
}

/* ============================================================
 * Step motor once in chosen direction
 * ============================================================
 * fwd = true  -> forward
 * fwd = false -> reverse
 *
 * DIR should be stable before STEP pulse is sent.
 */
void step_motor(bool fwd)
{
    gpio_put(DIR_PIN, fwd);    // set direction pin
    sleep_us(10);              // small settle time for DIR
    send_one_pulse();          // send one step pulse
}

/* ============================================================
 * Move motor by multiple steps
 * ============================================================
 * Repeats single-step function 'steps' times.
 */
void move_n_steps(int steps, bool fwd)
{
    for (int i = 0; i < steps; i++)
    {
        step_motor(fwd);
    }
}

/* ============================================================
 * Set DRV8825 microstepping mode
 * ============================================================
 * MODE2:MODE1:MODE0 encodes:
 *   000 = 1
 *   001 = 2
 *   010 = 4
 *   011 = 8
 *   100 = 16
 *   101 = 32
 *
 * Invalid values are ignored here; parser should reject them first.
 */
void set_microstepping(int microsteps)
{
    switch (microsteps)
    {
    case 1:
        gpio_put(M0_PIN, 0); gpio_put(M1_PIN, 0); gpio_put(M2_PIN, 0);
        break;
    case 2:
        gpio_put(M0_PIN, 1); gpio_put(M1_PIN, 0); gpio_put(M2_PIN, 0);
        break;
    case 4:
        gpio_put(M0_PIN, 0); gpio_put(M1_PIN, 1); gpio_put(M2_PIN, 0);
        break;
    case 8:
        gpio_put(M0_PIN, 1); gpio_put(M1_PIN, 1); gpio_put(M2_PIN, 0);
        break;
    case 16:
        gpio_put(M0_PIN, 0); gpio_put(M1_PIN, 0); gpio_put(M2_PIN, 1);
        break;
    case 32:
        gpio_put(M0_PIN, 1); gpio_put(M1_PIN, 0); gpio_put(M2_PIN, 1);
        break;
    default:
        return;   // do nothing for invalid values
    }

    microstepping = microsteps;  // remember current mode
    sleep_us(10);                // allow MODE pins to settle
}

/* ============================================================
 * Read terminal input without blocking
 * ============================================================
 * Uses getchar_timeout_us(0):
 * - returns a character if one is waiting
 * - returns PICO_ERROR_TIMEOUT if no input is available
 *
 * This function:
 * - stores typed characters in command_buffer
 * - handles Enter/newline
 * - handles backspace
 * - echoes valid characters back to terminal
 *
 * It does NOT parse commands or move the motor.
 */
void process_input(void)
{
    int ch = getchar_timeout_us(0);   // non-blocking read

    /* Keep reading until no more characters are waiting */
    while (ch != PICO_ERROR_TIMEOUT)
    {
        /* Ignore new input if a full command is already waiting
         * to be processed in main() */
        if (!flag_complete)
        {
            switch (ch)
            {
            /* -------- Enter / newline --------
             * End the command, add null terminator, set flag_complete.
             * '\0' marks end of C string.
             */
            case '\r':
            case '\n':
                if (buffer_index > 0)
                {
                    command_buffer[buffer_index] = '\0';
                    flag_complete = true;
                    printf("\r\n");   // move to next line on terminal
                }
                break;

            /* -------- Backspace --------
             * If there is at least one character in the buffer:
             * - move back one index
             * - erase character on terminal using "\b \b"
             */
            case '\b':
            case 127:
                if (buffer_index > 0)
                {
                    buffer_index--;
                    printf("\b \b");
                }
                break;

            /* -------- Normal character --------
             * Only accept letters, digits, and spaces.
             * Also keep 1 spare byte for '\0' terminator.
             */
            default:
                if ((isalnum(ch) || ch == ' ') &&
                    (buffer_index < BUFFER_SIZE - 1))
                {
                    command_buffer[buffer_index] = (char)ch;
                    buffer_index++;
                    putchar(ch);   // echo typed character
                }
                break;
            }
        }

        /* Check for another queued character */
        ch = getchar_timeout_us(0);
    }
}

/* ============================================================
 * Parse one command and execute it
 * ============================================================
 * buffer          = command string to parse
 * report          = output message buffer
 * report_buf_size = size of report buffer
 *
 * Returns:
 *   true  = command was valid and executed
 *   false = invalid command or parameter error
 *
 * Important sscanf idea:
 * - return value = how many items matched
 * - trailing %c is used to catch extra junk
 *
 * Example:
 *   "fwd 100"   -> matches steps only
 *   "fwd 100 x" -> extra %c also matches, so invalid
 */
bool process_command(const char *buffer, char *report, int report_buf_size)
{
    int steps, time_us, mode_value;
    char type[8];   // enough for "high"/"low" plus '\0'
    char extra;     // catches unexpected extra input
    int n;          // sscanf return value

    /* --------------------------------------------------------
     * fwd <steps>
     * -------------------------------------------------------- */
    n = sscanf(buffer, "fwd %d %c", &steps, &extra);

    /* n == 1 means only the expected integer matched */
    if (n == 1)
    {
        if (steps < 0 || steps > 2000)
        {
            snprintf(report, report_buf_size,
                     "steps parameter out of range: expected 0-2000, got %d",
                     steps);
            return false;
        }

        move_n_steps(steps, true);
        snprintf(report, report_buf_size,
                 "fwd: moved by %d steps", steps);
        return true;
    }

    /* n == 2 means extra junk was present after valid parameter */
    if (n == 2)
    {
        snprintf(report, report_buf_size, "invalid command");
        return false;
    }

    /* --------------------------------------------------------
     * back <steps>
     * -------------------------------------------------------- */
    n = sscanf(buffer, "back %d %c", &steps, &extra);

    if (n == 1)
    {
        if (steps < 0 || steps > 2000)
        {
            snprintf(report, report_buf_size,
                     "steps parameter out of range: expected 0-2000, got %d",
                     steps);
            return false;
        }

        move_n_steps(steps, false);
        snprintf(report, report_buf_size,
                 "back: moved by %d steps", steps);
        return true;
    }

    if (n == 2)
    {
        snprintf(report, report_buf_size, "invalid command");
        return false;
    }

    /* --------------------------------------------------------
     * delay <high|low> <time_us>
     * -------------------------------------------------------- */
    n = sscanf(buffer, "delay %7s %d %c", type, &time_us, &extra);

    /* %7s prevents overflow into type[8] */
    if (n == 2)
    {
        /* First validate type */
        if (strcmp(type, "high") != 0 && strcmp(type, "low") != 0)
        {
            snprintf(report, report_buf_size,
                     "type parameter out of range: expected high or low, got %s",
                     type);
            return false;
        }

        /* Then validate time range */
        if (time_us < 0 || time_us > 1000000)
        {
            snprintf(report, report_buf_size,
                     "delay parameter out of range: expected 0-1000000, got %d",
                     time_us);
            return false;
        }

        /* Apply new timing */
        if (strcmp(type, "high") == 0)
        {
            high_delay = time_us;
            snprintf(report, report_buf_size,
                     "high delay set to %d us", time_us);
        }
        else
        {
            low_delay = time_us;
            snprintf(report, report_buf_size,
                     "low delay set to %d us", time_us);
        }

        return true;
    }

    /* n == 3 means extra junk after the expected arguments */
    if (n == 3)
    {
        snprintf(report, report_buf_size, "invalid command");
        return false;
    }

    /* --------------------------------------------------------
     * mode <1|2|4|8|16|32>
     * -------------------------------------------------------- */
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
        snprintf(report, report_buf_size,
                 "microsteps set to %d", mode_value);
        return true;
    }

    if (n == 2)
    {
        snprintf(report, report_buf_size, "invalid command");
        return false;
    }

    /* If no pattern matched, command is invalid */
    snprintf(report, report_buf_size, "invalid command");
    return false;
}

/* ============================================================
 * main
 * ============================================================
 * - Initialise stdio and GPIO
 * - Repeatedly collect input
 * - When Enter completes a command, parse and execute it
 * - Print result
 * - Reset buffer for next command
 */
int main(void)
{
    char report_buf[100];   // stores success/error message from parser

    stdio_init_all();       // enable serial terminal I/O
    init_pins();            // initialise stepper GPIO pins

    printf("Stepper Motor Control Initialized\r\n");

    while (true)
    {
        process_input();    // collect characters without blocking

        /* Only process once a complete command is ready */
        if (flag_complete)
        {
            process_command(command_buffer,
                            report_buf,
                            (int)sizeof(report_buf));

            printf("%s\r\n", report_buf);

            /* Reset command buffer state for next command */
            buffer_index = 0;
            command_buffer[0] = '\0';
            flag_complete = false;
        }
    }
}

/* ============================================================
 * Quick memory helpers
 * ============================================================
 * '\0'                  = end of C string
 * buffer_index          = next empty slot in command_buffer
 * flag_complete         = Enter pressed; command ready to parse
 * getchar_timeout_us(0) = non-blocking input
 * sscanf return value   = number of successfully matched items
 * trailing %c           = catches extra junk after valid args
 * %7s                   = safe width limit for type[8]
 * snprintf              = safe print into buffer
 * "\b \b"               = erase one character on terminal
 * "\r\n"                = Windows-style newline for terminal
 */