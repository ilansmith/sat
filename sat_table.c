#include "sat_table.h"
#include <stdlib.h>

#define MAX(m, n) ((m)<(n) ? (n) : (m))

static literal_t *literal_alloc(int id, sat_tv_t tv)
{
	literal_t *literal;

	if (!(literal = calloc(1, sizeof(literal_t))))
		return NULL;

	literal->id = id;
	literal->tv = tv;
	return literal;
}

static void literal_free(literal_t *literal)
{
	free(literal);
}

static int literal_new(clause_t *clause, char *name, sat_tv_t tv,
		variable_t **variables)
{
	int id = 0, cnt = 1;
	literal_t **literals = &clause->literals;

	if (variables && (id = variable_add(variables, name)) == -1)
		return -1;

	while (*literals) {
		if (id && (*literals)->id == id) {
			if ((*literals)->tv != tv)
				(*literals)->tv = SAT_TV_TAUTOLOGY;

			return 0;
		}

		literals = &(*literals)->next;
		cnt++;
	}

	clause->dim++;
	return (*literals = literal_alloc(id ? id : cnt, tv)) ? 0 : -1;
}

void literal_clr(literal_t *literals)
{
	literal_t *tmp;

	while ((tmp = literals)) {
		literals = literals->next;
		literal_free(tmp);
	}
}

static clause_t *clause_alloc(int id)
{
	clause_t *clause;

	if (!(clause = calloc(1, sizeof(clause_t))))
		return NULL;
	clause->id = id;
	return clause;
}

static void clause_free(clause_t *clause)
{
	literal_clr(clause->literals);
	free(clause);
}

int clause_new(clause_t **clauses)
{
	int id = 1;

	while (*clauses) {
		clauses = &(*clauses)->next;
		id++;
	}

	return (*clauses = clause_alloc(id)) ? 0 : -1;
}

int clause_add(clause_t *clause, char *name, int val, variable_t **variables)
{
	while (clause->next)
		clause = clause->next;

	if (literal_new(clause, name, val ?
		SAT_TV_TRUE : SAT_TV_FALSE, variables)) {
		return -1;
	}

	return 0;
}

void clause_clr(clause_t **clauses)
{
	clause_t *tmp;

	while ((tmp = *clauses)) {
		*clauses = (*clauses)->next;
		clause_free(tmp);
	}
}

static int clause_num_cb(int n, clause_t *cls)
{
	return n + 1;
}

static int clause_dim_cb(int n, clause_t *cls)
{
	return MAX(n, cls->dim);
}

static int clause_cnt(clause_t *clauses, int(*cb)(int n, clause_t *cls))
{
	clause_t *cls;
	int n = 0;

	for (cls = clauses; cls; cls = cls->next)
		n = cb(n, cls);
	return n;
}

int clause_num_get(clause_t *clauses)
{
	return clause_cnt(clauses, clause_num_cb);
}

int clause_dim_get(clause_t *clauses)
{
	return clause_cnt(clauses, clause_dim_cb);
}

void table_clr(table_t *table)
{
	if (!table)
		return;

	if (table->t) {
		int i;

		for (i = 0; i < table->record_num; i++) {
			if (!table->t[i])
				break;
			free(table->t[i]);
		}
		free(table->t);
	}

	if (table->assignments) {
		int i;

		for (i = 0; i < table->assignment_num; i++)
			free(table->assignments[i]);
		free(table->assignments);
	}

	free(table);
}

static table_t *table_alloc(int record_num, int record_dim)
{
	table_t *table;
	int i;

	if (!(table = calloc(1, sizeof(table_t))))
		goto Error;

	table->record_num = record_num;
	table->record_dim = record_dim;

	if (!(table->t = calloc(record_num, sizeof(literal_t *))))
		goto Error;

	for (i = 0; i < record_num; i++) {
		int j;

		if (!(table->t[i] = calloc(record_dim, sizeof(literal_t))))
			goto Error;

		for (j = 0; j < record_dim; j++) {
			table->t[i][j].id = 0;
			table->t[i][j].tv = SAT_TV_TAUTOLOGY;
		}
	}

	return table;

Error:
	table_clr(table);
	return NULL;
}

static int column_sort_compare(int id1, int id2)
{
	if (!id1)
		return 0;

	if (!id2)
		return 1;

	return id1 <= id2;
}

static void column_sort_merge(table_t *table, int clause, int lower_start,
		int lower_end, int higher_start, int higher_end)
{
	int curr = lower_start, *remaining_start = NULL, *remaining_end = NULL;

	while (lower_start <= lower_end && higher_start <= higher_end) {
		if (column_sort_compare(table->t[lower_start][clause].id,
			table->t[higher_start][clause].id)) {
			table->t[curr][0] = table->t[lower_start][clause];
			lower_start++;
			remaining_start = &higher_start;
			remaining_end = &higher_end;
		}
		else {
			table->t[curr][0] = table->t[higher_start][clause];
			higher_start++;
			remaining_start = &lower_start;
			remaining_end = &lower_end;
		}

		curr++;
	}

	if (!remaining_start || !remaining_end)
		return;

	while (*remaining_start <= *remaining_end) {
		table->t[curr][0] = table->t[*remaining_start][clause];
		(*remaining_start)++;
		curr++;
	}
}

static void column_sort_rec(table_t *table, int clause, int start, int end)
{
	int mid, curr;

	if (start >= end)
		return;

	mid = (end + start)/2;
	column_sort_rec(table, clause, start, mid);
	column_sort_rec(table, clause, mid + 1, end);
	column_sort_merge(table, clause, start, mid, mid + 1, end);
	for (curr = start; curr <=end; curr++)
		table->t[curr][clause] = table->t[curr][0];
}

static void columns_sort(table_t *table)
{
	int i;

	for (i = 1; i < table->record_dim; i++)
		column_sort_rec(table, i, 0, table->record_num - 1);
	for (i = 0; i < table->record_num; i++) {
		table->t[i][0].id = 0;
		table->t[i][0].tv = SAT_TV_INVERSE(SAT_TV_TAUTOLOGY);
	}
}

table_t *table_create(clause_t *clauses, int var_num)
{
	table_t *table;
	int i, record_num = clause_dim_get(clauses);
	int j, record_dim = clause_num_get(clauses);
	clause_t *cls;

	if (!(table = table_alloc(record_num, record_dim + 1)))
		return NULL;

	table->var_num = var_num;
	for (i = 0; i < record_num; i++)
		table->t[i][0].tv = SAT_TV_INVERSE(SAT_TV_TAUTOLOGY);

	for (cls = clauses, i = 1; cls; cls = cls->next, i++) {
		literal_t *lit;

		for (lit = cls->literals, j = 0; lit; lit = lit->next, j++) {
			table->t[j][i].id = lit->id;
			table->t[j][i].tv = SAT_TV_INVERSE(lit->tv);
		}
	}

	columns_sort(table);
	return table;
}

int table_assignment_init(table_t *table, int assignment_num)
{
	int i;

	if (!(table->assignments = calloc(assignment_num, sizeof(literal_t *))))
		return -1;

	for (i = 0; i < assignment_num; i++) {
		if (!(table->assignments[i] = calloc(table->var_num,
			sizeof(literal_t)))) {
			literal_t *tmp;

			for (tmp = table->assignments[0]; tmp; tmp++)
				free(tmp);
			free(table->assignments);
			return -1;
		}
	}

	table->assignment_num = assignment_num;
	return 0;
}

void table_assignment_insert(table_t *table, int assignment, int id,
	sat_tv_t tv)
{
	int idx = id -1;

	table->assignments[assignment][idx].id = id;
	table->assignments[assignment][idx].tv = tv;
}

