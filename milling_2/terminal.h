/** \file terminal.h
 *  \defgroup pico_term
 *
 * Header-only code for simple terminal escape codes.
 * For use with printf(), as provided by pico_stdio.
 */

#ifndef CC2511_TERMINAL_H
#define CC2511_TERMINAL_H

#include <stdio.h>

/* Colour constants */
#define clrBlack   30U
#define clrRed     31U
#define clrGreen   32U
#define clrYellow  33U
#define clrBlue    34U
#define clrMagenta 35U
#define clrCyan    36U
#define clrWhite   37U

/* Proper null-terminated ANSI escape strings */
static const char term_data_esc_prefix[] = "\x1B[";
static const char term_data_cls[] = "2J";

/*! Clear terminal */
static inline int term_cls(void)
{
    return printf("%s%s", term_data_esc_prefix, term_data_cls);
}

/*! Move cursor to x,y */
static inline int term_move_to(unsigned short x, unsigned short y)
{
    return printf("%s%d;%dH", term_data_esc_prefix, y, x);
}

/*! Set foreground/background colours */
static inline int term_set_color(unsigned short foreground, unsigned short background)
{
    return printf("%s0;%d;%dm",
                  term_data_esc_prefix,
                  foreground,
                  background + ((background < 40) ? 10 : 0));
}

/*! Erase current line */
static inline int term_erase_line(void)
{
    return printf("%sK", term_data_esc_prefix);
}

/*! Bell */
static inline void term_bell(void)
{
    printf("\a");
}

#endif /* CC2511_TERMINAL_H */