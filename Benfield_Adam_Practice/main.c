/**************************************************************                                                    /**************************************************************
 * PRACTICE EXAM QUICK REFERENCE (Q1 – Q6)
 *
 * Q1 - Project setup / startup text
 *      a) Create new Pico project
 *      b) Initialise stdio
 *      c) Print:
 *         CC2511 Exam 2026
 *         Your Name
 *
 * Q2 - Start game / LEDs / random flashes
 *      a) init_buttons() using array of 3 switch pins
 *      b) Wait for any pushbutton to start game
 *      c) init_leds() red + green as simple ON/OFF outputs
 *      d) Generate random number n = 1 to 5
 *      e) show_number(n):
 *            green flashes (n-1) times
 *            red flashes once
 *      f) Prompt user to enter digit
 *
 * Q3 - Read terminal answer (polling)
 *      a) Use getchar_timeout_us(0)
 *      b) Buffer typed chars until Enter
 *      c) Convert text to integer using atoi()
 *      d) Compare answer with n
 *      e) Print Right! / Wrong!
 *      f) Brief comment: polling acceptable here
 *
 * Q4 - Time limit / scoring / replay
 *      a) 3 second time limit to answer
 *      b) If timeout -> "Too slow!"
 *      c) score starts at 0
 *         correct = +1
 *         wrong / timeout = -1
 *      d) If score reaches +5 -> win
 *         If score reaches -5 -> lose
 *      e) Pause 5 sec then start new game
 *
 * Q5 - Blue LED PWM feedback
 *      a) Configure BLUE_PIN for PWM
 *      b) Brightness increases as score nears win/loss
 *      c) Use score^2 style scaling
 *
 * Q6 - ADC + terminal layout
 *      a) Read LDR using ADC
 *      b) If too dark -> "Good night - time to stop"
 *      c) Use terminal.h formatted screen layout
 *
 * CORE FUNCTIONS TO REMEMBER
 *      stdio_init_all();
 *      gpio_init(pin);
 *      gpio_set_dir(pin, GPIO_IN / GPIO_OUT);
 *      gpio_pull_up(pin);
 *      gpio_get(pin);
 *      gpio_put(pin,1/0);
 *      rand()%5 + 1;
 *      getchar_timeout_us(0);
 *      atoi(buffer);
 *      sleep_ms(x);
 *      get_absolute_time();
 *
 **************************************************************/
/**************************************************************
 * main.c
 * rev 1.0 21-Apr-2026 skegm
 * Benfield_Adam_Practice
 *
 * Code description/aim:
 * A basic counting game for very young children.
 * Q1 - a)
 **************************************************************/

/**************************************************************
 * main.c
 * rev 1.0 21-Apr-2026 skegm
 * Benfield_Adam_Practice
 *
 * Code description/aim:
 * A basic counting game for very young children.
 **************************************************************/

#include <stdbool.h> // bool, true, false
#include <stdint.h>  // uint16_t etc.
#include <stdlib.h>  // rand(), srand(), atoi(), abs()
#include <stdio.h>   // printf()

#include "pico/stdlib.h" // basic Pico SDK functions: stdio, sleep, GPIO basics
#include "pico/time.h"   // get_absolute_time(), to_ms_since_boot(), absolute_time_diff_us()

#include "hardware/pwm.h" // PWM control for LED brightness
#include "hardware/adc.h" // ADC functions for reading analog values

#include "terminal.h" // terminal screen helpers

// Q2 - c)
/* -- Dev board pin numbers --------------------------------- */
#define RED_PIN 11
#define GREEN_PIN 12
#define BLUE_PIN 13

// Q2 - a) pushbutton pins
#define SWITCH_1 2
#define SWITCH_2 3
#define SWITCH_3 4

// Q6 - a) LDR / ADC
#define LDR_GPIO 26
#define LDR_ADC_INPUT 0

/* -- Game constants ---------------------------------------- */
// Q4 - a), c), d)
#define ANSWER_TIMEOUT_MS 3000
#define WIN_SCORE 5
#define LOSE_SCORE -5

// Q5 - a), b), c)
#define PWM_WRAP 65535

// Q6 - b) adjust if needed for your board/room brightness
#define NIGHT_THRESHOLD 500
/*-----------------------------------------------------------*/

// Function prototypes
// Q2 - a), c), e)
void init_buttons(void);
void init_leds(void);
void show_number(int n);

// Q2 - b)
bool any_button_pressed(void);

// Q3 - a), Q4 - a)
int read_answer(void);

// Q5 - a), b), c)
void init_blue_pwm(void);
void show_score_feedback(int score);

// Q6 - a), b), c)
void init_light_sensor(void);
uint16_t read_light_level(void);
void draw_layout(void);
void display_status(int score, uint16_t light_value, const char *message);

int main(void)
{
  // Q1 - b)
  stdio_init_all(); // initialise standard input/output

  // Q2 - a)
  init_buttons();

  // Q2 - c)
  init_leds();

  // Q5 - a)
  init_blue_pwm();

  // Q6 - a)
  init_light_sensor();

  // Q6 - c)
  draw_layout();

  // Q1 - c)
  term_move_to(1, 1);
  printf("CC2511 Exam 2026");

  term_move_to(1, 2);
  printf("Adam Benfield");

  while (true) // start a new game after each win/loss
  {
    // Q4 - c)
    int score = 0;

    // Q2 - b)
    display_status(score, read_light_level(), "Press any pushbutton to start the game...");

    // Q2 - b)
    while (!any_button_pressed())
    {
      // wait
    }

    // Q2 - b)
    while (any_button_pressed())
    {
      // wait for release
    }

    // Q2 - b)
    display_status(score, read_light_level(), "Type in the number of flashes...");

    // Q2 - d)
    srand((int)to_ms_since_boot(get_absolute_time()));

    // Q4 - d)
    while (score > LOSE_SCORE && score < WIN_SCORE)
    {
      // Q6 - a)
      uint16_t light_value = read_light_level();

      // Q6 - b)
      if (light_value < NIGHT_THRESHOLD)
      {
        while (read_light_level() < NIGHT_THRESHOLD)
        {
          display_status(score, read_light_level(), "Good night - time to stop");
          sleep_ms(200);
        }

        display_status(score, read_light_level(), "Type in the number of flashes...");
      }

      // Q2 - d)
      int n = rand() % 5 + 1;

      // Q2 - e)
      show_number(n);

      // Q2 - f)
      display_status(score, read_light_level(), "Enter a digit now...");

      // Q3 - a), b), c)
      // Q4 - a)
      int answer = read_answer();

      // Q3 - d), e)
      // Q4 - b), c)
      if (answer == -2)
      {
        display_status(score, read_light_level(), "Too slow!");
        score--;
      }
      else if (answer == n)
      {
        display_status(score, read_light_level(), "Right!");
        score++;
      }
      else
      {
        display_status(score, read_light_level(), "Wrong!");
        score--;
      }

      // Q4 - c)
      display_status(score, read_light_level(), "Round complete");

      // Q5 - b), c)
      show_score_feedback(score);
      sleep_ms(500);
    }

    // Q4 - d)
    if (score >= WIN_SCORE)
    {
      display_status(score, read_light_level(), "You won!");
    }
    else
    {
      display_status(score, read_light_level(), "You lost!");
    }

    // Q4 - e)
    sleep_ms(5000);
  }
}

// Q2 - a)
void init_buttons(void)
{
  int button_pins[3] = {SWITCH_1, SWITCH_2, SWITCH_3};

  for (int i = 0; i < 3; i++)
  {
    gpio_init(button_pins[i]);
    gpio_set_dir(button_pins[i], GPIO_IN);
    gpio_pull_up(button_pins[i]);
  }
}

// Q2 - b)
bool any_button_pressed(void)
{
  return (gpio_get(SWITCH_1) == 0 ||
          gpio_get(SWITCH_2) == 0 ||
          gpio_get(SWITCH_3) == 0);
}

// Q2 - c)
void init_leds(void)
{
  int led_pins[2] = {RED_PIN, GREEN_PIN};

  for (int j = 0; j < 2; j++)
  {
    gpio_init(led_pins[j]);
    gpio_set_dir(led_pins[j], GPIO_OUT);
    gpio_put(led_pins[j], 0);
  }
}

// Q2 - e)
void show_number(int n)
{
  for (int i = 0; i < n - 1; i++)
  {
    gpio_put(GREEN_PIN, 1);
    sleep_ms(500);

    gpio_put(GREEN_PIN, 0);
    sleep_ms(500);
  }

  gpio_put(RED_PIN, 1);
  sleep_ms(500);

  gpio_put(RED_PIN, 0);
  sleep_ms(500);
}

// Q3b:
// Polling is acceptable here because the game only needs simple
// terminal input after each LED sequence, and the program can repeatedly
// check for input without the extra complexity of interrupt/event-based UART.

// Q3 - a), b), c)
// Q4 - a)
int read_answer(void)
{
  char buffer[16];
  int index = 0;

  // Q4 - a)
  absolute_time_t start_time = get_absolute_time();

  // Q6 - c)
  term_move_to(8, 9);
  term_erase_line();

  while (true)
  {
    // Q4 - a)
    if (absolute_time_diff_us(start_time, get_absolute_time()) >= ANSWER_TIMEOUT_MS * 1000)
    {
      return -2;
    }

    // Q3 - a)
    int ch = getchar_timeout_us(0);

    if (ch != PICO_ERROR_TIMEOUT)
    {
      // Q3 - b)
      if (ch == '\r' || ch == '\n')
      {
        buffer[index] = '\0';

        if (index == 0)
        {
          return -1;
        }

        // Q3 - c)
        return atoi(buffer);
      }
      else if (ch == '\b' || ch == 127)
      {
        if (index > 0)
        {
          index--;
          term_move_to(8 + index, 9);
          printf(" ");
          term_move_to(8 + index, 9);
        }
      }
      else if (ch >= '0' && ch <= '9' && index < 15)
      {
        buffer[index] = (char)ch;
        term_move_to(8 + index, 9);
        printf("%c", ch);
        index++;
      }
    }
  }
}

// Q5 - a)
void init_blue_pwm(void)
{
  gpio_set_function(BLUE_PIN, GPIO_FUNC_PWM);

  uint slice_num = pwm_gpio_to_slice_num(BLUE_PIN);

  pwm_set_wrap(slice_num, PWM_WRAP);
  pwm_set_gpio_level(BLUE_PIN, 0);
  pwm_set_enabled(slice_num, true);
}

// Q5 - b), c)
void show_score_feedback(int score)
{
  int magnitude = abs(score);
  int brightness = magnitude * magnitude;

  uint16_t pwm_level = (brightness * PWM_WRAP) / 25;

  pwm_set_gpio_level(BLUE_PIN, pwm_level);
  sleep_ms(2000);
  pwm_set_gpio_level(BLUE_PIN, 0);
}

// Q6 - a)
void init_light_sensor(void)
{
  adc_init();
  adc_gpio_init(LDR_GPIO);
  adc_select_input(LDR_ADC_INPUT);
}

// Q6 - a)
uint16_t read_light_level(void)
{
  adc_select_input(LDR_ADC_INPUT);
  return adc_read();
}

// Q6 - c)
void draw_layout(void)
{
  term_set_color(clrWhite, clrBlack);
  term_cls();

  term_move_to(1, 1);
  printf("CC2511 Exam 2026");

  term_move_to(1, 2);
  printf("Adam Benfield");

  term_move_to(1, 4);
  printf("Score:");

  term_move_to(1, 5);
  printf("Light:");

  term_move_to(1, 7);
  printf("Status:");

  term_move_to(1, 9);
  printf("Input:");
}

// Q6 - c)
void display_status(int score, uint16_t light_value, const char *message)
{
  term_move_to(8, 4);
  term_erase_line();
  printf("%d", score);

  term_move_to(8, 5);
  term_erase_line();
  printf("%u", light_value);

  term_move_to(9, 7);
  term_erase_line();
  printf("%s", message);

  term_move_to(8, 9);
}