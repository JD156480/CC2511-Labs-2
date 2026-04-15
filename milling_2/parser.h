#ifndef __PARSER_H__
#define __PARSER_H__

#include <stdbool.h>

/* -----------------------------------------------------------
 * command_type_t
 * -----------------------------------------------------------
 * Identifies which command was parsed from the input string.
 * CMD_INVALID means the parser could not recognise the command.
 */
typedef enum
{
    CMD_INVALID = 0,

    /* Motion commands */
    CMD_G0,      /* rapid move */
    CMD_G1,      /* linear move */
    CMD_G2,      /* clockwise arc */
    CMD_G3,      /* counter-clockwise arc */
    CMD_G4,      /* dwell */

    /* Unit / mode commands */
    CMD_G20,     /* inches */
    CMD_G21,     /* millimeters */
    CMD_G28,     /* go to home */
    CMD_G281,    /* set home */
    CMD_G90,     /* absolute mode */
    CMD_G91,     /* relative mode */

    /* Spindle / mode switching */
    CMD_M2,      /* return to manual mode */
    CMD_M3,      /* spindle on */
    CMD_M5,      /* spindle off */

    /* Help */
    CMD_HELP
} command_type_t;

/* -----------------------------------------------------------
 * parsed_command_t
 * -----------------------------------------------------------
 * Stores a fully parsed command.
 *
 * 'type' says what command it is.
 * 'has_x', 'has_y', etc. tell you whether each parameter
 * was actually present in the input.
 *
 * Example:
 *   G1 X10 Y20 F100
 *
 * would give:
 *   type  = CMD_G1
 *   has_x = true,  x = 10
 *   has_y = true,  y = 20
 *   has_f = true,  f = 100
 *   all other has_* flags = false
 */
typedef struct
{
    command_type_t type;

    bool has_x;
    bool has_y;
    bool has_z;
    bool has_f;
    bool has_i;
    bool has_j;
    bool has_p;
    bool has_s;

    float x;
    float y;
    float z;
    float f;
    float i;
    float j;
    float p;
    float s;
} parsed_command_t;

/* Parse one command string into a structured command.
 * Returns true if successful, false if invalid.
 */
bool parse_command(const char *buffer, parsed_command_t *out);

#endif // __PARSER_H__