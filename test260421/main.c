/**************************************************************
 * main.c
 * rev 1.0 21-Apr-2026 skegm
 * test260421
 * ***********************************************************/

#include "pico/stdlib.h"
#include <stdio.h>
#include <stdbool.h>

int main(void) {
    stdio_init_all();

    while (true) {
        printf("Test260421 is running\r\n");
        sleep_ms(1000);
    }
}