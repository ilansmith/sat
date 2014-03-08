#include "sat.h"
#include "unit_test.h"
#include "util.h"
#include "char_stream.h"
#include "sat_variable.h"
#include "sat_table.h"
#include "sat_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define MAX_CELLULAR_SPACE_LENGTH 20
#define MAX_CELLULAR_APACE_HIGHT 8

#define COL_REGISTER_START(c) \
	static char col[10]; \
	int brightness, colour; \
	switch (c) \
	{

#define COL_REGISTER(sym, b, c) \
	case (sym): \
		brightness = b; \
		colour = c; \
		break;

#define COL_REGISTER_END \
	default: \
		brightness = ATTR_DULL; \
		colour = COL_WHITE; \
		break; \
	} \
	sprintf(col, "\033[%d;%d;%dm", brightness, colour, COL_BCK_BLACK); \
	printf("%s%c%s", col, c, COL_CLEAR); \
	fflush(stdout);

typedef struct sat_test_event_t {
	int ret;
} sat_test_event_t;

typedef enum {
	COL_TEST_ITER_0 = 0,
	COL_TEST_ITER_43 = 1,
	COL_TEST_ITER_76 = 2,
	COL_TEST_ITER_150 = 3,
} sat_colour_test_t;

typedef struct print_table_t {
	literal_t *record;
	int dim;
} print_table_t;

typedef struct print_neighbour_t {
	coordinate_euclid_t *record;
	int dim;
	int n;
	int m;
} print_neighbour_t;

typedef struct sat_colour_test_entry_t {
	int iteration;
	void(*test)(void);
} sat_colour_test_entry_t;

static variable_t *test_variables;
static clause_t *test_clauses;
static table_t *test_table;
static code2str_t trueth_values[] = {
	{SAT_TV_FALSE, "false"},
	{SAT_TV_TRUE, "true"},
	{SAT_TV_TAUTOLOGY, "tautology"},
	{SAT_TV_PARADOX, "paradox"},
	{-1}
};

static void test_print(int is_error, char *(* p_func)(char *str, void *data),
	void *data_expected, void *data_recieved)
{
	char str[256];

	if (is_error)
		p_comment(p_func(str, data_expected), "expected: ");
	p_comment(p_func(str, data_recieved), is_error ? "received: " : " ");
}

static char *print_variables_str(char *str, void *data)
{
	variable_t *var = (variable_t *)data;

	sprintf(str, "%%sid: %d, name: %s", var->id, var->name);
	return str;
}

static void print_variables(int is_error, variable_t *var_expected,
	variable_t *var_recieved)
{
	test_print(is_error, print_variables_str, var_expected, var_recieved);
}

static char *literal_cat(char *str, literal_t *lit)
{
	char tmp[32];

	sprintf(tmp, "id: %d, tv: %-11s", lit->id, code2str(trueth_values,
		lit->tv));
	strcat(str, tmp);
	return str;
}

static char *print_literals_str(char *str, void *data)
{
	literal_t *lit = (literal_t *)data;

	sprintf(str, "%%-16s");
	literal_cat(str, lit);
	return str;
}

static void print_literals(int is_error, literal_t *lit_expected,
	literal_t *lit_recieved)
{
	test_print(is_error, print_literals_str, lit_expected, lit_recieved);
}

static char *print_clauses_str(char *str, void *data)
{
	clause_t *cls = (clause_t *)data;

	sprintf(str, "%%sid: %d, dim: %d, literals:", cls->id, cls->dim);
	return str;
}

static void print_clauses(int is_error, clause_t *cls_expected,
	clause_t *cls_recieved)
{
	test_print(is_error, print_clauses_str, cls_expected, cls_recieved);
}

static char *print_table_str(char *str, void *data)
{
	print_table_t *tbl = (print_table_t *)data;
	int i;

	sprintf(str, "%%s");
	for (i = 0; i < tbl->dim; i++)
		literal_cat(str, &tbl->record[i]);
	return str;
}

static void print_table(int is_error, int dim, literal_t *rec_expected,
	literal_t *rec_recieved)
{
	print_table_t expected;
	print_table_t recieved;

	expected.record = rec_expected;
	expected.dim = dim;
	recieved.record = rec_recieved;
	recieved.dim = dim;

	test_print(is_error, print_table_str, &expected, &recieved);
}

static void print_coordinate_table(coordinate_euclid_t **table, int hight,
	int length)
{
	int i, j;
	char arg[10], curr[10], line[32];

	p_comment("coordinate table:");
	for (i = 0; i < hight; i++) {
		sprintf(line, " ");
		for (j = 0; j < length; j++) {
			sprintf(curr, "%d,%d", table[i][j].n, table[i][j].m);
			sprintf(arg, "%-7s", curr);
			strcat(line, arg);
		}

		p_comment(line);
	}
}

static void print_neighbour_table_headers(char *first, ...)
{
	va_list ap;
	char line[256], curr[10], *arg;

	sprintf(line, " %-9s", first);
	va_start(ap, first);
	while ((arg = va_arg(ap, char *))) {
		sprintf(curr, "%-7s",arg);
		strcat(line, curr);
	}
	va_end(ap);

	p_comment(line);
}

static void print_neighbour_header(void)
{
	p_comment("neighbour table:");
	print_neighbour_table_headers("cell", "north", "ne", "east", "se",
		"south", "sw", "west", "nw", NULL);
}

static char *print_neighbour_str(char *str, void *data)
{
	print_neighbour_t *cell = (print_neighbour_t *)data;
	char curr[10], arg[16];
	int i;

	sprintf(str, "%%s");
	sprintf(curr, "[%d][%d]:", cell->n, cell->m);
	sprintf(arg, "%-9s", curr);
	strcat(str, arg);
	for (i = 0; i < cell->dim; i++) {
		sprintf(curr, "%d,%d", cell->record[i].n, cell->record[i].m);
		sprintf(arg, "%-7s", curr);
		strcat(str, arg);
	}

	return str;
}

static void print_neighbours(int is_error, int dim, int n, int m,
	coordinate_euclid_t *rec_expected, coordinate_euclid_t *rec_recieved)
{
	print_neighbour_t expected;
	print_neighbour_t recieved;

	expected.record = rec_expected;
	expected.dim = dim;
	expected.n = n;
	expected.m = m;
	recieved.record = rec_recieved;
	recieved.dim = dim;
	recieved.n = n;
	recieved.m = m;

	test_print(is_error, print_neighbour_str, &expected, &recieved);
}

static int assert_variables(variable_t *expected_vars, int sz)
{
	variable_t *var;
	int i, error_code = 0, ret = 0;

	p_comment("variables:");
	for (var = test_variables, i = 0; var && i < sz; var = var->next, i++) {
		int print_error = 0;

		if (!error_code && (var->id != expected_vars[i].id ||
			strcmp(var->name, expected_vars[i].name))) {
			error_code = 1;
			print_error = 1;
		}
		print_variables(print_error, &expected_vars[i], var);
	}

	if (i < sz || var)
		error_code += 2;

	if (error_code) {
		p_comment("variable assertion error code: %d", error_code);
		ret = -1;
	}

	return ret;
}

static int assert_clauses(clause_t *expected_clauses, int sz)
{
	clause_t *cls;
	int clause_error = 0, literal_error = 0, literal_error_found = 0,
		ret = 0;
	int i;

	p_comment("clauses:");
	for (cls = test_clauses, i = 0; cls && i < sz; cls = cls->next, i++) {
		literal_t *lit;
		literal_t *expected_literals = expected_clauses[i].literals;
		int j, print_error_clause = 0;

		if (!clause_error && (cls->id != expected_clauses[i].id ||
			cls->dim != expected_clauses[i].dim)) {
			clause_error = 1;
			print_error_clause = 1;
		}
		print_clauses(print_error_clause, &expected_clauses[i], cls);

		for (lit = cls->literals,
			j = 0; lit && j < expected_clauses[i].dim;
			lit = lit->next, j++) {
			int error_print_literal = 0;

			if (!literal_error_found &&
				(lit->id != expected_literals[j].id ||
				lit->tv != expected_literals[j].tv)) {
				literal_error = 1;
				error_print_literal = 1;
			}
			print_literals(error_print_literal,
				&expected_literals[j], lit);
		}

		if (!literal_error_found &&
			(j < expected_clauses[i].dim || lit)) {
			literal_error += 2;
		}

		if (literal_error)
			literal_error_found = 1;
	}

	if (i < sz || cls)
		clause_error += 2;

	if (clause_error || literal_error) {
		p_comment("clause assertion error code: %d", clause_error);
		p_comment("literal assertion error code: %d", literal_error);
		ret = -1;
	}

	return ret;
}

static int assert_table(table_t *expected_table)
{
	int i; int j;
	int is_error = 0;

	if (expected_table->record_num != test_table->record_num) {
		p_comment("record num: expected: %d, got: %d",
			expected_table->record_num, test_table->record_num);
		is_error = 1;
		goto Exit;
	}

	if (expected_table->record_dim != test_table->record_dim) {
		p_comment("record dim: expected: %d, got: %d",
			expected_table->record_dim, test_table->record_dim);
		is_error = 1;
		goto Exit;
	}

	p_comment("table:");
	for (i = 0; i < expected_table->record_num; i++) {
		int print_error = 0;

		for (j = 0; j < expected_table->record_dim; j++) {
			if (!is_error &&
				(expected_table->t[i][j].id !=
				test_table->t[i][j].id ||
				expected_table->t[i][j].tv !=
				test_table->t[i][j].tv)) {
				is_error = 1;
				print_error = 1;
			}
		}
		print_table(print_error, expected_table->record_dim,
			expected_table->t[i], test_table->t[i]);
	}

Exit:
	return is_error ? -1 : 0;
}

static int assert_neighbours(coordinate_euclid_t **table, int hight,
	int length, coordinate_euclid_t **expected,
	coordinate_euclid_t **recieved)
{
	int i, j, dim_x, dim_y, ret = 0;

	dim_x = 8; /* number of neighbours per cell */
	dim_y = hight * length; /* total number of cells */

	print_coordinate_table(table, hight, length);
	print_neighbour_header();
	for (i = 0; i < dim_y; i++) {
		int print_error = 0;

		for (j = 0; j < dim_x; j++) {
			if (!ret && (expected[i][j].n != recieved[i][j].n ||
				expected[i][j].m != recieved[i][j].m)) {
				ret = -1;
				print_error = 1;
			}
		}
		print_neighbours(print_error, dim_x, i/hight, i%length,
			expected[i], recieved[i]);
	}

	return ret;
}

static int test_variable_new(char *name, int val)
{
	return variable_add(&test_variables, name) == -1 ? -1 : 0;
}

static void test_clause_new(void)
{
	clause_new(&test_clauses);
}

static int test_literal_new(char *name, int val)
{
	return clause_add(test_clauses, name, val, NULL);
}

static int test_literal_new_w_var(char *name, int val)
{
	return clause_add(test_clauses, name, val, &test_variables);
}

static void test_uninit(void *o)
{
	variables_clr(&test_variables);
	clause_clr(&test_clauses);
	table_clr(test_table);
}

/* success callback */
static void parse_success(void *o)
{
	((sat_test_event_t *)o)->ret = 0;
	event_add(test_uninit, o);
}

/* fail callback */
static void parse_fail(void *o)
{
	((sat_test_event_t *)o)->ret = -1;
	event_add(test_uninit, o);
}

static void parse_should_fail_success(void *o)
{
	parse_fail(o);
}

static void parse_should_fail_fail(void *o)
{
	parse_success(o);
}

/* generic cnf parse test */
static int test_sat_parser(void (*clause_new)(void),
	int (*literal_new)(char *name, int val), event_func_t success,
	event_func_t fail, char *cnf)
{
	sat_test_event_t t;
	cs_t *cs;
	parser_cb_t parser_cb = {
		.clause_new = clause_new,
		.literal_new = literal_new,
		.success_cb = success,
		.success_data = &t,
		.fail_cb = fail,
		.fail_data = &t,
	};

	p_comment("test expression: \"%s\"", cnf);
	cs = cs_open(CS_STRING2CHAR, cnf);
	event_init();
	parser_init(&parser_cb, cs);
	event_loop();
	event_uninit();
	cs_close(cs);

	return t.ret;
}

static int test_sat_parser_should_fail(char *cnf)
{
	return test_sat_parser(NULL, NULL, parse_should_fail_success,
		parse_should_fail_fail, cnf);
}

static int test_sat_parser_should_succeed(char *cnf)
{
	return test_sat_parser(NULL, NULL, parse_success, parse_fail, cnf);
}

/* colour tests */
static void p_sat_colours(char c)
{
	COL_REGISTER_START(c)
	COL_REGISTER('G', ATTR_BRIGHT, COL_RED);
	COL_REGISTER('L', ATTR_DULL, COL_YELLOW);
	COL_REGISTER('A', ATTR_BRIGHT, COL_GREEN);
	COL_REGISTER('E', ATTR_BRIGHT, COL_BLUE);
	COL_REGISTER('F', ATTR_DULL, COL_MAGENTA);
	COL_REGISTER('D', ATTR_BRIGHT, COL_WHITE);
	COL_REGISTER('0', ATTR_BRIGHT, COL_YELLOW);
	COL_REGISTER('1', ATTR_BRIGHT, COL_CYAN);
	COL_REGISTER('o', ATTR_BRIGHT, COL_BLACK);
	/* border symbols */
	COL_REGISTER('|', ATTR_BRIGHT, COL_WHITE);
	COL_REGISTER('-', ATTR_BRIGHT, COL_WHITE);
	COL_REGISTER('+', ATTR_BRIGHT, COL_WHITE);
	COL_REGISTER_END
}

static void p_sat_buf(char *buf)
{
	int i;

	for (i = 0; i < strlen(buf); i++)
		p_sat_colours(buf[i]);
	printf("\n");
}

static void p_sat(char *str)
{
	char s[MAX_CELLULAR_SPACE_LENGTH];
	int i;

	snprintf(s, MAX_CELLULAR_SPACE_LENGTH, "| %s", str);
	for (i = strlen(s); i < MAX_CELLULAR_SPACE_LENGTH - 2; i++)
		s[i] = ' ';
	s[MAX_CELLULAR_SPACE_LENGTH - 2] = '|';
	s[MAX_CELLULAR_SPACE_LENGTH - 1] = 0;
	p_sat_buf(s);
}

static void register_row_num(int num)
{
	int i, extra = MAX_CELLULAR_APACE_HIGHT - num;

	if (extra < 0)
		return;

	for (i = 0; i < extra; i++)
		p_sat("");
}

static void p_sat_boundary_row(void)
{
	char s[MAX_CELLULAR_SPACE_LENGTH];
	int i;

	s[0] = '+';
	s[MAX_CELLULAR_SPACE_LENGTH - 2] = '+';
	s[MAX_CELLULAR_SPACE_LENGTH - 1] = 0;

	for (i = 1; i < MAX_CELLULAR_SPACE_LENGTH - 2; i++)
		s[i] = '-';

	p_sat_buf(s);
}

static void col_test_iter_0(void)
{
	register_row_num(3);
	p_sat("AAo");
	p_sat("A o");
	p_sat("LGGoo");
}

static void col_test_iter_43(void)
{
	register_row_num(5);
	p_sat("  o");
	p_sat("  F");
	p_sat("ooG L1A");
	p_sat("A G G A");
	p_sat("A0L GFE");
}

static void col_test_iter_76(void)
{
	register_row_num(7);
	p_sat("AAo");
	p_sat("0 o");
	p_sat("LGG");
	p_sat("  D");
	p_sat("GGL AAo GGL");
	p_sat("o 0 1 o   1");
	p_sat("oAA LGGooAA");
}

static void col_test_iter_150(void)
{
	register_row_num(7);
	p_sat("ooG L10");
	p_sat("0 G G 1");
	p_sat("10L Goo");
	p_sat("");
	p_sat("   ooG L11 ooG");
	p_sat("   0 G G 0 1 G");
	p_sat("   01L Goo 11L");
}

static sat_colour_test_entry_t colour_tests[] = {
	[ COL_TEST_ITER_0 ] = { 0, col_test_iter_0 },
	[ COL_TEST_ITER_43 ] = { 43, col_test_iter_43 },
	[ COL_TEST_ITER_76 ] = { 76, col_test_iter_76 },
	[ COL_TEST_ITER_150 ] = { 150, col_test_iter_150 },
};

static void test_colours(sat_colour_test_t id)
{
	p_comment("(p1 or p2) and (p1 or -p3)");
	p_sat_boundary_row();
	colour_tests[id].test();
	p_sat_boundary_row();
	p_comment("iteration %d", colour_tests[id].iteration);
}

/* illegal expressions */
static int test01(void)
{
	return test_sat_parser_should_fail("");
}

static int test02(void)
{
	return test_sat_parser_should_fail("[)");
}

static int test03(void)
{
	return test_sat_parser_should_fail("()");
}

static int test04(void)
{
	return test_sat_parser_should_fail("(a b)");
}

static int test05(void)
{
	return test_sat_parser_should_fail("(or b)");
}

static int test06(void)
{
	return test_sat_parser_should_fail("(a or)");
}

static int test07(void)
{
	return test_sat_parser_should_fail("(a or - b)");
}

static int test08(void)
{
	return test_sat_parser_should_fail("(a or --b)");
}

static int test09(void)
{
	return test_sat_parser_should_fail("(a or 5b)");
}

static int test10(void)
{
	return test_sat_parser_should_fail("(a or b*c)");
}

static int test11(void)
{
	return test_sat_parser_should_fail("(a or -b*c)");
}

static int test12(void)
{
	return test_sat_parser_should_fail("(a or -bc)^");
}

static int test13(void)
{
	return test_sat_parser_should_fail("(a or -bc) (-a or c)");
}

static int test14(void)
{
	return test_sat_parser_should_fail("(a or -bc) andd (-a or c)");
}

static int test15(void)
{
	return test_sat_parser_should_fail("(a or -bc)and (-a or c)");
}

static int test16(void)
{
	return test_sat_parser_should_fail("(a or -bc) and(-a or c)");
}

static int test17(void)
{
	return test_sat_parser_should_fail("(") ||
		test_sat_parser_should_fail("(  ");
}

static int test18(void)
{
	return test_sat_parser_should_fail("(a") ||
		test_sat_parser_should_fail("(-a  ");
}

static int test19(void)
{
	return test_sat_parser_should_fail("(a or") ||
		test_sat_parser_should_fail("(-a  or   ");
}

static int test20(void)
{
	return test_sat_parser_should_fail("(a or -") ||
		test_sat_parser_should_fail("(-a  or -   ");
}

static int test21(void)
{
	return test_sat_parser_should_fail("(a or b) and") ||
		test_sat_parser_should_fail("(-a  or b) and   ");
}

/* legal expressions */
static int test22(void)
{
	return test_sat_parser_should_succeed("(a)") ||
		test_sat_parser_should_succeed("  (  a  )  ");
}

static int test23(void)
{
	return test_sat_parser_should_succeed("(a or -b)") ||
		test_sat_parser_should_succeed("  (  a    or  -b )  ");
}

static int test24(void)
{
	return test_sat_parser_should_succeed("(a or -b) and (-c)") ||
		test_sat_parser_should_succeed("  (  a   or  -b  )   and   "
		"(  -c )  ");
}

static int test25(void)
{
	return test_sat_parser_should_succeed("(a or -b) and (-c or -a)");
}

static void test26_success(void *o)
{
	int res;
	variable_t expected_vars[4] = {
		{.id=1, .name="a"},
		{.id=2, .name="b"},
		{.id=3, .name="c"},
		{.id=4, .name="d"},
	};
	if ((res = assert_variables(expected_vars, ARRAY_SZ(expected_vars))))
		parse_fail(o);
	else
		parse_success(o);
}

static int test26(void)
{
	return test_sat_parser(NULL, test_variable_new, test26_success,
		parse_fail,
		"(a or b) and (-b or c or d) and (-b or -d) and (-c)");
}

static void test27_success(void *o)
{
	literal_t expected_literals1[2] = {
		{.id=1, .tv=SAT_TV_TRUE},
		{.id=2, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals2[3] = {
		{.id=1, .tv=SAT_TV_FALSE},
		{.id=2, .tv=SAT_TV_TRUE},
		{.id=3, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals3[2] = {
		{.id=1, .tv=SAT_TV_FALSE},
		{.id=2, .tv=SAT_TV_FALSE},
	};
	literal_t expected_literals4[1] = {
		{.id=1, .tv=SAT_TV_FALSE},
	};

	clause_t expected_clauses[4] = {
		{.id = 1, .dim = 2, .literals = expected_literals1},
		{.id = 2, .dim = 3, .literals = expected_literals2},
		{.id = 3, .dim = 2, .literals = expected_literals3},
		{.id = 4, .dim = 1, .literals = expected_literals4},
	};

	if (assert_clauses(expected_clauses, ARRAY_SZ(expected_clauses)))
		parse_fail(o);
	else
		parse_success(o);
}

static int test27(void)
{
	return test_sat_parser(test_clause_new, test_literal_new,
		test27_success, parse_fail,
		"(a or b) and (-b or c or d) and (-b or -d) and (-c)");
}

static void test28_success(void *o)
{
	variable_t expected_vars[4] = {
		{.id=1, .name="a"},
		{.id=2, .name="b"},
		{.id=3, .name="c"},
		{.id=4, .name="d"},
	};
	literal_t expected_literals1[2] = {
		{.id=1, .tv=SAT_TV_TRUE},
		{.id=2, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals2[3] = {
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=3, .tv=SAT_TV_TRUE},
		{.id=4, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals3[2] = {
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=4, .tv=SAT_TV_FALSE},
	};
	literal_t expected_literals4[1] = {
		{.id=3, .tv=SAT_TV_FALSE},
	};
	clause_t expected_clauses[4] = {
		{.id = 1, .dim = 2, .literals = expected_literals1},
		{.id = 2, .dim = 3, .literals = expected_literals2},
		{.id = 3, .dim = 2, .literals = expected_literals3},
		{.id = 4, .dim = 1, .literals = expected_literals4},
	};

	if (assert_variables(expected_vars, ARRAY_SZ(expected_vars)) ||
		assert_clauses(expected_clauses, ARRAY_SZ(expected_clauses))) {
		parse_fail(o);
	}
	else {
		parse_success(o);
	}
}

static int test28(void)
{
	return test_sat_parser(test_clause_new, test_literal_new_w_var,
		test28_success, parse_fail,
		"(a or b) and (-b or c or d) and (-b or -d) and (-c)");
}

static void test29_success(void *o)
{
	variable_t expected_vars[4] = {
		{.id=1, .name="a"},
		{.id=2, .name="b"},
		{.id=3, .name="c"},
		{.id=4, .name="d"},
	};
	literal_t expected_literals1[2] = {
		{.id=1, .tv=SAT_TV_TRUE},
		{.id=2, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals2[3] = {
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=3, .tv=SAT_TV_TRUE},
		{.id=4, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals3[2] = {
		{.id=2, .tv=SAT_TV_TAUTOLOGY},
		{.id=4, .tv=SAT_TV_FALSE},
	};
	literal_t expected_literals4[1] = {
		{.id=3, .tv=SAT_TV_FALSE},
	};
	clause_t expected_clauses[4] = {
		{.id = 1, .dim = 2, .literals = expected_literals1},
		{.id = 2, .dim = 3, .literals = expected_literals2},
		{.id = 3, .dim = 2, .literals = expected_literals3},
		{.id = 4, .dim = 1, .literals = expected_literals4},
	};

	if (assert_variables(expected_vars, ARRAY_SZ(expected_vars)) ||
		assert_clauses(expected_clauses, ARRAY_SZ(expected_clauses))) {
		parse_fail(o);
	}
	else {
		parse_success(o);
	}
}

static int test29(void)
{
	return test_sat_parser(test_clause_new, test_literal_new_w_var,
		test29_success, parse_fail,
		"(a or b or b) and (-b or c or d) and (-b or -d or b) and "
		"(-c or -c)");
}

static void test30_success(void *o)
{
	variable_t expected_vars[4] = {
		{.id=1, .name="a"},
		{.id=2, .name="b"},
		{.id=3, .name="c"},
		{.id=4, .name="d"},
	};
	literal_t expected_literals1[2] = {
		{.id=1, .tv=SAT_TV_TRUE},
		{.id=2, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals2[3] = {
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=3, .tv=SAT_TV_TRUE},
		{.id=4, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals3[2] = {
		{.id=2, .tv=SAT_TV_TAUTOLOGY},
		{.id=4, .tv=SAT_TV_FALSE},
	};
	literal_t expected_literals4[1] = {
		{.id=3, .tv=SAT_TV_FALSE},
	};
	clause_t expected_clauses[4] = {
		{.id = 1, .dim = 2, .literals = expected_literals1},
		{.id = 2, .dim = 3, .literals = expected_literals2},
		{.id = 3, .dim = 2, .literals = expected_literals3},
		{.id = 4, .dim = 1, .literals = expected_literals4},
	};
	literal_t expected_literal_array1[4+1] = {
		{.id=0, .tv=SAT_TV_PARADOX},
		{.id=1, .tv=SAT_TV_FALSE},
		{.id=2, .tv=SAT_TV_TRUE},
		{.id=2, .tv=SAT_TV_PARADOX},
		{.id=3, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literal_array2[4+1] = {
		{.id=0, .tv=SAT_TV_PARADOX},
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=3, .tv=SAT_TV_FALSE},
		{.id=4, .tv=SAT_TV_TRUE},
		{.id=0, .tv=SAT_TV_TAUTOLOGY},
	};
	literal_t expected_literal_array3[4+1] = {
		{.id=0, .tv=SAT_TV_PARADOX},
		{.id=0, .tv=SAT_TV_TAUTOLOGY},
		{.id=4, .tv=SAT_TV_FALSE},
		{.id=0, .tv=SAT_TV_TAUTOLOGY},
		{.id=0, .tv=SAT_TV_TAUTOLOGY},
	};
	literal_t *expected_literal_table[3] = {
		expected_literal_array1,
		expected_literal_array2,
		expected_literal_array3,
	};
	table_t expected_table = {
		.record_num=3,
		.record_dim=5,
		.t=expected_literal_table,
	};

	if (assert_variables(expected_vars, ARRAY_SZ(expected_vars)) ||
		assert_clauses(expected_clauses, ARRAY_SZ(expected_clauses))) {
		parse_fail(o);
		return;
	}

	test_table = table_create(test_clauses, ARRAY_SZ(expected_vars));

	if (assert_table(&expected_table))
		parse_fail(o);
	else
		parse_success(o);
}

static int test30(void)
{
	return test_sat_parser(test_clause_new, test_literal_new_w_var,
		test30_success, parse_fail,
		"(a or b or b) and (-b or c or d) and (-b or -d or b) and "
		"(-c or -c)");
}

static void test31_success(void *o)
{
	variable_t expected_vars[4] = {
		{.id=1, .name="a"},
		{.id=2, .name="b"},
		{.id=3, .name="c"},
		{.id=4, .name="d"},
	};
	literal_t expected_literals1[4] = {
		{.id=1, .tv=SAT_TV_TRUE},
		{.id=2, .tv=SAT_TV_TRUE},
		{.id=3, .tv=SAT_TV_TRUE},
		{.id=4, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals2[4] = {
		{.id=3, .tv=SAT_TV_FALSE},
		{.id=1, .tv=SAT_TV_TRUE},
		{.id=4, .tv=SAT_TV_FALSE},
		{.id=2, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals3[4] = {
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=1, .tv=SAT_TV_FALSE},
		{.id=3, .tv=SAT_TV_TRUE},
		{.id=4, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals4[4] = {
		{.id=4, .tv=SAT_TV_FALSE},
		{.id=3, .tv=SAT_TV_FALSE},
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=1, .tv=SAT_TV_FALSE},
	};
	literal_t expected_literals5[4] = {
		{.id=4, .tv=SAT_TV_FALSE},
		{.id=3, .tv=SAT_TV_TRUE},
		{.id=1, .tv=SAT_TV_TRUE},
		{.id=2, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literals6[3] = {
		{.id=4, .tv=SAT_TV_TRUE},
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=1, .tv=SAT_TV_TRUE},
	};
	clause_t expected_clauses[6] = {
		{.id = 1, .dim = 4, .literals = expected_literals1},
		{.id = 2, .dim = 4, .literals = expected_literals2},
		{.id = 3, .dim = 4, .literals = expected_literals3},
		{.id = 4, .dim = 4, .literals = expected_literals4},
		{.id = 5, .dim = 4, .literals = expected_literals5},
		{.id = 6, .dim = 3, .literals = expected_literals6},
	};
	literal_t expected_literal_array1[6+1] = {
		{.id=0, .tv=SAT_TV_PARADOX},
		{.id=1, .tv=SAT_TV_FALSE},
		{.id=1, .tv=SAT_TV_FALSE},
		{.id=1, .tv=SAT_TV_TRUE},
		{.id=1, .tv=SAT_TV_TRUE},
		{.id=1, .tv=SAT_TV_FALSE},
		{.id=1, .tv=SAT_TV_FALSE},
	};
	literal_t expected_literal_array2[6+1] = {
		{.id=0, .tv=SAT_TV_PARADOX},
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=2, .tv=SAT_TV_TRUE},
		{.id=2, .tv=SAT_TV_TRUE},
		{.id=2, .tv=SAT_TV_FALSE},
		{.id=2, .tv=SAT_TV_TRUE},
	};
	literal_t expected_literal_array3[6+1] = {
		{.id=0, .tv=SAT_TV_PARADOX},
		{.id=3, .tv=SAT_TV_FALSE},
		{.id=3, .tv=SAT_TV_TRUE},
		{.id=3, .tv=SAT_TV_FALSE},
		{.id=3, .tv=SAT_TV_TRUE},
		{.id=3, .tv=SAT_TV_FALSE},
		{.id=4, .tv=SAT_TV_FALSE},
	};
	literal_t expected_literal_array4[6+1] = {
		{.id=0, .tv=SAT_TV_PARADOX},
		{.id=4, .tv=SAT_TV_FALSE},
		{.id=4, .tv=SAT_TV_TRUE},
		{.id=4, .tv=SAT_TV_FALSE},
		{.id=4, .tv=SAT_TV_TRUE},
		{.id=4, .tv=SAT_TV_TRUE},
		{.id=0, .tv=SAT_TV_TAUTOLOGY},
	};
	literal_t *expected_literal_table[4] = {
		expected_literal_array1,
		expected_literal_array2,
		expected_literal_array3,
		expected_literal_array4,
	};
	table_t expected_table = {
		.record_num=4,
		.record_dim=7,
		.t=expected_literal_table,
	};

	if (assert_variables(expected_vars, ARRAY_SZ(expected_vars)) ||
		assert_clauses(expected_clauses, ARRAY_SZ(expected_clauses))) {
		parse_fail(o);
		return;
	}

	test_table = table_create(test_clauses, ARRAY_SZ(expected_vars));

	if (assert_table(&expected_table))
		parse_fail(o);
	else
		parse_success(o);
}

static int test31(void)
{
	return test_sat_parser(test_clause_new, test_literal_new_w_var,
		test31_success, parse_fail,
		"(a or b or c or d) and (-c or a or -d or b) and "
		"(-b or -a or c or d) and (-d or -c or -b or -a) and "
		"(-d or c or a or b) and (d or -b or a)");
}

static int test32(void)
{
	int i, j, l;
	coordinate_euclid_t co_table0[3] = {{0, 0}, {0, 1}, {0, 2}};
	coordinate_euclid_t co_table1[3] = {{1, 0}, {1, 1}, {1, 2}};
	coordinate_euclid_t co_table2[3] = {{2, 0}, {2, 1}, {2, 2}};
	coordinate_euclid_t *co_table[3] = {
		co_table0,
		co_table1,
		co_table2,
	};
	coordinate_euclid_t expected_00[8] = {{2, 0}, {2, 1}, {0, 1}, {1, 1},
		{1, 0}, {1, 2}, {0, 2}, {2 ,2}};
	coordinate_euclid_t expected_01[8] = {{2, 1}, {2, 2}, {0, 2}, {1, 2},
		{1, 1}, {1, 0}, {0, 0}, {2 ,0}};
	coordinate_euclid_t expected_02[8] = {{2, 2}, {2, 0}, {0, 0}, {1, 0},
		{1, 2}, {1, 1}, {0, 1}, {2 ,1}};
	coordinate_euclid_t expected_10[8] = {{0, 0}, {0, 1}, {1, 1}, {2, 1},
		{2, 0}, {2, 2}, {1, 2}, {0 ,2}};
	coordinate_euclid_t expected_11[8] = {{0, 1}, {0, 2}, {1, 2}, {2, 2},
		{2, 1}, {2, 0}, {1, 0}, {0 ,0}};
	coordinate_euclid_t expected_12[8] = {{0, 2}, {0, 0}, {1, 0}, {2, 0},
		{2, 2}, {2, 1}, {1, 1}, {0 ,1}};
	coordinate_euclid_t expected_20[8] = {{1, 0}, {1, 1}, {2, 1}, {0, 1},
		{0, 0}, {0, 2}, {2, 2}, {1 ,2}};
	coordinate_euclid_t expected_21[8] = {{1, 1}, {1, 2}, {2, 2}, {0, 2},
		{0, 1}, {0, 0}, {2, 0}, {1 ,0}};
	coordinate_euclid_t expected_22[8] = {{1, 2}, {1, 0}, {2, 0}, {0, 0},
		{0, 2}, {0, 1}, {2, 1}, {1 ,1}};
	coordinate_euclid_t *expected[9] = {
		expected_00,
		expected_01,
		expected_02,
		expected_10,
		expected_11,
		expected_12,
		expected_20,
		expected_21,
		expected_22,
	};
	coordinate_euclid_t recieved_00[8], recieved_01[8], recieved_02[8],
		recieved_10[8], recieved_11[8], recieved_12[8], recieved_20[8],
		recieved_21[8], recieved_22[8];
	coordinate_euclid_t *recieved[9] = {
		recieved_00,
		recieved_01,
		recieved_02,
		recieved_10,
		recieved_11,
		recieved_12,
		recieved_20,
		recieved_21,
		recieved_22,
	};

	for (i = 0, l = 0; i < 3; i++) {
		for (j = 0; j < 3 && l < 9; j++, l++) {
			wind_dir_t dir;

			for (dir = DIR_NO; dir <= DIR_NW; dir++) {
				coordinate_moore_neighbour(3, 3,
					&co_table[i][j], dir,
					&recieved[l][dir]);
			}
		}
	}

	return assert_neighbours(co_table, 3, 3, expected, recieved);
}

static int test51(void)
{
	test_colours(COL_TEST_ITER_0);
	return 0;
}

static int test52(void)
{
	test_colours(COL_TEST_ITER_43);
	return 0;
}

static int test53(void)
{
	test_colours(COL_TEST_ITER_76);
	return 0;
}

static int test54(void)
{
	test_colours(COL_TEST_ITER_150);
	return 0;
}

/* test array */
static test_t sat_tests[] =
{
	{
		description: "empty expression",
		func: test01,
	},
	{
		description: "illegal left paren",
		func: test02,
	},
	{
		description: "empty clause",
		func: test03,
	},
	{
		description: "literal not separated by \"or\"",
		func: test04,
	},
	{
		description: "no literal preceding \"or\"",
		func: test05,
	},
	{
		description: "no literal following \"or\"",
		func: test06,
	},
	{
		description: "negation not adjacent to variable",
		func: test07,
	},
	{
		description: "double negation",
		func: test08,
	},
	{
		description: "illegal prefix",
		func: test09,
	},
	{
		description: "illegal character",
		func: test10,
	},
	{
		description: "illegal character in negated expression",
		func: test11,
	},
	{
		description: "non white space after left parens",
		func: test12,
	},
	{
		description: "no \"and\" between clauses",
		func: test13,
	},
	{
		description: "bad \"and\" token between clauses",
		func: test14,
	},
	{
		description: "\"and\" adjacent to left clause",
		func: test15,
	},
	{
		description: "\"and\" adjacent to right clause",
		func: test16,
	},
	{
		description: "early termination after left paren",
		func: test17,
	},
	{
		description: "early termination after literal",
		func: test18,
	},
	{
		description: "early termination after \"or\"",
		func: test19,
	},
	{
		description: "early termination after negation",
		func: test20,
	},
	{
		description: "early termination after \"and\"",
		func: test21,
	},
	{
		description: "single literal in a clause",
		func: test22,
	},
	{
		description: "multiple literals in a clause",
		func: test23,
	},
	{
		description: "clauses of different sizes",
		func: test24,
	},
	{
		description: "multiple use of variables in different clauses",
		func: test25,
	},
	{
		description: "expression variables",
		func: test26,
	},
	{
		description: "expression clauses",
		func: test27,
	},
	{
		description: "expression clauses with variable accumulation",
		func: test28,
	},
	{
		description: "multiple variable entries in a clause",
		func: test29,
	},
	{
		description: "position table creation",
		func: test30,
	},
	{
		description: "position table creation with column sorting",
		func: test31,
	},
	{
		description: "neighbours",
		func: test32,
	},
	{
		description: "colour combinations - iteration 0",
		func: test51,
#ifndef COLOURS
		disabled: 1,
#endif
	},
	{
		description: "colour combinations - iteration 43",
		func: test52,
#ifndef COLOURS
		disabled: 1,
#endif
	},
	{
		description: "colour combinations - iteration 76",
		func: test53,
#ifndef COLOURS
		disabled: 1,
#endif
	},
	{
		description: "colour combinations - iteration 150",
		func: test54,
#ifndef COLOURS
		disabled: 1,
#endif
	},
	{0},
};

int main(int argc, char *argv[])
{
	return unit_test(argc, argv, ARRAY_SZ(sat_tests), sat_tests);
}

