/**************************************************************
 * parser.c
 * Convert one text command string into a parsed_command_t.
 **************************************************************/

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include "parser.h"

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

static void make_uppercase_copy(const char *src, char *dst, int size)
{
    int i;

    for (i = 0; i < size - 1 && src[i] != '\0'; i++)
    {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }

    dst[i] = '\0';
}

/* Get first token only, e.g. "G1" from "G1 X10 Y20" */
static void extract_first_token(const char *src, char *token, int size)
{
    int i = 0;

    while (*src != '\0' && isspace((unsigned char)*src))
    {
        src++;
    }

    while (*src != '\0' &&
           !isspace((unsigned char)*src) &&
           i < size - 1)
    {
        token[i++] = *src++;
    }

    token[i] = '\0';
}

static command_type_t parse_command_type(const char *buffer)
{
    char token[16];

    extract_first_token(buffer, token, sizeof(token));

    if (strcmp(token, "H") == 0 || strcmp(token, "HELP") == 0)
        return CMD_HELP;

    if (strcmp(token, "G28.1") == 0)
        return CMD_G281;

    if (strcmp(token, "G28") == 0)
        return CMD_G28;

    if (strcmp(token, "G0") == 0 || strcmp(token, "G00") == 0)
        return CMD_G0;

    if (strcmp(token, "G1") == 0 || strcmp(token, "G01") == 0)
        return CMD_G1;

    if (strcmp(token, "G2") == 0 || strcmp(token, "G02") == 0)
        return CMD_G2;

    if (strcmp(token, "G3") == 0 || strcmp(token, "G03") == 0)
        return CMD_G3;

    if (strcmp(token, "G4") == 0 || strcmp(token, "G04") == 0)
        return CMD_G4;

    if (strcmp(token, "G20") == 0)
        return CMD_G20;

    if (strcmp(token, "G21") == 0)
        return CMD_G21;

    if (strcmp(token, "G90") == 0)
        return CMD_G90;

    if (strcmp(token, "G91") == 0)
        return CMD_G91;

    if (strcmp(token, "M2") == 0 || strcmp(token, "M02") == 0)
        return CMD_M2;

    if (strcmp(token, "M3") == 0)
        return CMD_M3;

    if (strcmp(token, "M5") == 0)
        return CMD_M5;

    return CMD_INVALID;
}

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