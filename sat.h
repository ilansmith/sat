#ifndef _SAT_H_
#define _SAT_H_

#define MAX_VARIABLE_SZ 256
#define SAT_TV_INVERSE(val) ((SAT_TV_CNT + 1 - (val)) % SAT_TV_CNT)

/* special escape sequences */
#define CLEAR_SCREEN "\E[2J"
#define CURSOR_DISSABLE "\E[?25l"
#define CURSOR_ENABLE "\E[?25h"

/* cursor movement and colouring escape sequence formats */
#define FMT_CURSOR_UP "\E[%dA"
/* The Color Code: <ESC>[{attr};{fg};{bg}m */
#define FMT_COLOUR "\E[%d;%d;%dm"
#define FMT_COLOUR_SIZE 12

/* colour brightness */
#define ATTR_DULL 0
#define ATTR_BRIGHT 1

/* colours */
#define COL_BCK_BLACK 40
#define COL_WHITE 29
#define COL_BLACK 30
#define COL_RED 31
#define COL_GREEN 32
#define COL_YELLOW 33
#define COL_BLUE 34
#define COL_MAGENTA 35
#define COL_CYAN 36
#define COL_GREY 37
#define COL_CLEAR "\E[00;00;00m"

/* iteration rates */
#define SPEED_SLOW 500000
#define SPEED_MEDIUM 100000
#define SPEED_RAPID 50000

#define SHIFT_LEFT(idx) (1<<idx)

#define ASCII_COPYRIGHT 169
#define ASCII_DOT 46
#define ASCII_ONE 49

typedef enum {
	SAT_TV_FALSE = 0,
	SAT_TV_TRUE = 1,
	SAT_TV_TAUTOLOGY = 2,
	SAT_TV_PARADOX = 3,
	SAT_TV_CNT = 4
} sat_tv_t;

int sat_init(int argc, char **argv);

#endif

