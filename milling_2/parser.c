/**************************************************************
 * parser.c
 * Convert one text command string into a parsed_command_t.
 *
 * Responsibilities:
 * - identify command type (G0, G1, G28, M3, etc.)
 * - extract optional parameters (X, Y, Z, F, I, J, P, S)
 * - store them in a structured output
 **************************************************************/

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "parser.h"

/* Reset parsed command to a clean default state */
static void init_parsed_command(parsed_command_t *cmd)
{
    cmd->type = CMD_INVALID;

    cmd->has_x = false;
    cmd->has_y = false;
    cmd->has_z = false;
    cmd->has_f = false;
    cmd->has_i = false;
    cmd->has_j = false;
    cmd->has_p = false;
    cmd->has_s = false;

    cmd->x = 0.0f;
    cmd->y = 0.0f;
    cmd->z = 0.0f;
    cmd->f = 0.0f;
    cmd->i = 0.0f;
    cmd->j = 0.0f;
    cmd->p = 0.0f;
    cmd->s = 0.0f;
}

/* Copy input into uppercase buffer so parsing is case-insensitive */
static void make_uppercase_copy(const char *src, char *dst, int size)
{
    int i;

    for (i = 0; i < size - 1 && src[i] != '\0'; i++)
    {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }

    dst[i] = '\0';
}

/* Identify the main command word at the start of the line */
static command_type_t parse_command_type(const char *buffer)
{
    if (strcmp(buffer, "H") == 0 || strcmp(buffer, "HELP") == 0)
        return CMD_HELP;

    if (strncmp(buffer, "G28.1", 5) == 0)
        return CMD_G281;

    if (strncmp(buffer, "G28", 3) == 0)
        return CMD_G28;

    if (strncmp(buffer, "G00", 3) == 0 || strncmp(buffer, "G0", 2) == 0)
        return CMD_G0;

    if (strncmp(buffer, "G01", 3) == 0 || strncmp(buffer, "G1", 2) == 0)
        return CMD_G1;

    if (strncmp(buffer, "G02", 3) == 0 || strncmp(buffer, "G2", 2) == 0)
        return CMD_G2;

    if (strncmp(buffer, "G03", 3) == 0 || strncmp(buffer, "G3", 2) == 0)
        return CMD_G3;

    if (strncmp(buffer, "G04", 3) == 0 || strncmp(buffer, "G4", 2) == 0)
        return CMD_G4;

    if (strcmp(buffer, "G20") == 0)
        return CMD_G20;

    if (strcmp(buffer, "G21") == 0)
        return CMD_G21;

    if (strcmp(buffer, "G90") == 0)
        return CMD_G90;

    if (strcmp(buffer, "G91") == 0)
        return CMD_G91;

    if (strcmp(buffer, "M2") == 0 || strcmp(buffer, "M02") == 0)
        return CMD_M2;

    if (strncmp(buffer, "M3", 2) == 0)
        return CMD_M3;

    if (strcmp(buffer, "M5") == 0)
        return CMD_M5;

    return CMD_INVALID;
}

/* Extract a numeric parameter after a given letter.
 * Example: find_value("G1 X10 Y20", 'X', &out) -> out = 10
 */
static bool extract_value(const char *buffer, char key, float *out)
{
    const char *ptr = strchr(buffer, key);

    if (ptr == NULL)
    {
        return false;
    }

    *out = strtof(ptr + 1, NULL);
    return true;
}

bool parse_command(const char *buffer, parsed_command_t *out)
{
    char upper[64];

    init_parsed_command(out);
    make_uppercase_copy(buffer, upper, sizeof(upper));

    out->type = parse_command_type(upper);

    if (out->type == CMD_INVALID)
    {
        return false;
    }

    /* Extract optional parameters if present */
    out->has_x = extract_value(upper, 'X', &out->x);
    out->has_y = extract_value(upper, 'Y', &out->y);
    out->has_z = extract_value(upper, 'Z', &out->z);
    out->has_f = extract_value(upper, 'F', &out->f);
    out->has_i = extract_value(upper, 'I', &out->i);
    out->has_j = extract_value(upper, 'J', &out->j);
    out->has_p = extract_value(upper, 'P', &out->p);
    out->has_s = extract_value(upper, 'S', &out->s);

    return true;
}