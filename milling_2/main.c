/**************************************************************
 * main.c 08/04/2026 CNS02 Adam, Faye
 * 
 *
 * High-level machine control:
 * - manual mode key controls
 * - command mode / G-code parsing
 * - position, home and unit tracking
 * - calls mmhal.c for low-level hardware actions
 * Analogy for memory: This is the ships pilot. We're
 * the captain telling the pilot what to do, and they
 * use mmhal.h to control mmhal.c to do the thing.
 *
 * Now only:
 * startup
 * init
 * main loop
 * mode switching logic
 * 
 *************************************************************/

#include <stdio.h>
#include "pico/stdlib.h"
#include "mmhal.h"
#include "machine.h"
#include "manual.h"
#include "command.h"


///////////////////////////////////////////////////////////////////////
// Main program
///////////////////////////////////////////////////////////////////////

int main(void)
{
  // Initialise USB serial and hardware layer
  stdio_init_all();
  sleep_ms(2000);
printf("Init OK\n");
  mmhal_init();

  printf("Init OK\n");
  print_manual_menu();

  while (true)
  {
    // Run the active control mode
    if (manual_mode)
    {
      handle_manual_mode();
    }
    else
    {
      handle_command_mode();
    }

    // Small delay to reduce CPU load
    sleep_ms(10);
  }
}