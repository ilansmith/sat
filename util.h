#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#define ARRAY_SZ(arr) (sizeof(arr) / sizeof(arr[0]))

typedef enum {
    DIR_NO = 0,
    DIR_NE = 1,
    DIR_EA = 2,
    DIR_SE = 3,
    DIR_SO = 4,
    DIR_SW = 5,
    DIR_WE = 6,
    DIR_NW = 7,
} wind_dir_t;

typedef struct code2code_t {
    int code;
    int val;
    int disabled;
} code2code_t;

typedef struct code2str_t {
    int code;
    char *str;
    int disabled;
} code2str_t;

typedef struct coordinate_euclid_t {
    int n;
    int m;
} coordinate_euclid_t;

int code2code(code2code_t *list, int code);
char *code2str(code2str_t *list, int code);

void coordinate_moore_neighbour(int hight, int length, coordinate_euclid_t *co, 
    wind_dir_t dir, coordinate_euclid_t *neighbour);

/* wind direction at offset from dir. positive values of offset for clockwise
 * and negative values of offset for anticlockwise. offset will always be
 * treated modulo 8.
 */
wind_dir_t wind_dir_offset(wind_dir_t dir, int offset);
#endif

