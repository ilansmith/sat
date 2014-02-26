#ifndef _SAT_CA_H_
#define _SAT_CA_H_

#include "sat_table.h"
#include "event.h"

typedef enum {
    SAT_PRINT_FIELD_CODE = 0,
    SAT_PRINT_FIELD_DIR = 1,
    SAT_PRINT_FIELD_FLAG = 2,
    SAT_PRINT_FIELD_COLOUR = 3,
    SAT_PRINT_FIELD_MONITOR = 4,
    SAT_PRINT_FIELD_NONE = 5
} sat_print_field_t;

typedef enum {
    SAT_PRINT_SPEED_SLOW = 0,
    SAT_PRINT_SPEED_MEDIUM = 1,
    SAT_PRINT_SPEED_FAST = 2
} sat_print_speed_t;

typedef struct {
    char representation;
    int colour;
    int is_bright;
} sat_print_t;

typedef struct ca_space_cb_t {
    table_t *table;
    event_func_t success_cb;
    void *success_data;
    event_func_t fail_cb;
    void *fail_data;
} ca_space_cb_t;

void sp_init(ca_space_cb_t *cb, table_t *tbl, sat_print_field_t print_field, 
    sat_print_speed_t print_speed);
void sat_print_params_get(void *o, int i, int j, sat_print_t *printer);

#endif

