#ifndef _SAT_TABLE_H_
#define _SAT_TABLE_H_

#include "sat.h"
#include "sat_variable.h"

typedef struct literal_t {
    struct literal_t *next;
    int id;
    sat_tv_t tv;
} literal_t;

typedef struct clause_t {
    struct clause_t *next;
    int id;
    int dim;
    literal_t *literals;
} clause_t;

typedef struct table_t {
    int record_num;
    int record_dim;
    literal_t **t;
    int var_num;
    int assignment_num;
    literal_t **assignments;
} table_t;

int clause_new(clause_t **clauses);
int clause_add(clause_t *clause, char *name, int val, variable_t **variables);
void clause_clr(clause_t **clauses);
int clause_num_get(clause_t *clauses);
int clause_dim_get(clause_t *clauses);
void table_clr(table_t *table);
table_t *table_create(clause_t *clauses, int var_num);
int table_assignment_init(table_t *table, int assignment_num);
void table_assignment_insert(table_t *table, int idx, int id, sat_tv_t tv);

#endif

