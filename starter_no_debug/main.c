/**************************************************************
 * main.c
 * rev 1.0 23-Jan-2026 Bruce
 * starter_no_debug
 * ***********************************************************/

#include "pico/stdlib.h"
#include <stdbool.h>
#include <stdio.h>
#define LED_PIN 25

int main(void) {
  // Initialise components and variables
  int count = 0;
  stdio_init_all();
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  printf("Program started\n");
  while (true) {
    // Repeated code here
    gpio_put(LED_PIN, !gpio_get(LED_PIN));  // Turn LED on
    printf("Count: %d\n", count);
    count++;
    sleep_ms(1000);
  }
}
