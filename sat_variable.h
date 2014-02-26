#ifndef _SAT_VARIABLE_H_
#define _SAT_VARIABLE_H_

#include "sat.h"

typedef struct variable_t {
    struct variable_t *next;
    int id;
    char name[MAX_VARIABLE_SZ];
} variable_t;

int variable_add(variable_t **variables, char *name);
void variables_clr(variable_t **variables);
char *literal_id2name(variable_t *variables, int id);

#endif
