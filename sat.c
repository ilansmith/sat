#include "event.h"
#include "char_stream.h"
#include "sat_args.h"
#include "sat_variable.h"
#include "sat_table.h"
#include "sat_parser.h"
#include "sat_ca.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define ASSERT_INPUT(arg, map) \
    do { \
	if (ICP(arg) & map) \
	{ \
	    int i, is_first = 1; \
	    fprintf(stderr, "option -%c is incompatible with", OPT(arg)); \
	    for (i = 0; i < IDX(INPUT_MAX); i++) \
	    { \
		if (!(map & 1<<i & ICP(arg))) \
		    continue; \
		fprintf(stderr, "%s -%c", is_first ? "" : ",", \
		    sat_opts[i].opt); \
		is_first = 0; \
	    } \
	    fprintf(stderr, "\n"); \
	    return -1; \
	} \
	map |= FLG(arg); \
    } while (0)

#define ASSERT_INPUT_SOURCE(arg, map) \
    do { \
	if ((FLG(arg) | ICP(arg)) & map) \
	{ \
	    fprintf(stderr, "only one CNF expression must be specified\n"); \
	    return -1; \
	} \
	if (!(sat.cs = cs_open(IDX(arg) == IDX(INPUT_STRING) ? \
	    CS_STRING2CHAR : CS_FILE2CHAR, optarg))) \
	{ \
	    fprintf(stderr, "could not open character stream for %s\n", \
		optarg); \
	    return -1; \
	} \
	map |= FLG(arg); \
    } while (0)

#define MICRO_DEVIDOR 1000000

typedef enum sat_error_t {
    SAT_ERR_PARSE = 0,
    SAT_ERR_POS_TABLE = 1,
    SAT_ERR_DO_SAT = 2,
} sat_error_t;

typedef struct sat_t {
    parser_cb_t parser_cb;
    ca_space_cb_t space;
    cs_t *cs;
    variable_t *variables;
    clause_t *clauses;
    table_t *table;
    sat_print_field_t print_field;
    sat_print_speed_t print_speed;
    int errno;
} sat_t;

void sat_print_results(table_t *table, variable_t *variables);
static sat_t sat;

static void sat_uninit(void *o)
{
    sat_t *s = (sat_t *)o;

    variables_clr(&(s->variables));
}

static void sat_success(void *o)
{
    sat_t *s = (sat_t *)o;

    sat_print_results(s->table, s->variables);
    table_clr(s->table);
    event_add(sat_uninit, o);
}

static void sat_error(void *o)
{
    sat_t *s = (sat_t *)o;

    switch (s->errno)
    {
    case SAT_ERR_PARSE:
	cs_close(s->cs);
	clause_clr(&(s->clauses));
	break;
    case SAT_ERR_POS_TABLE:
    case SAT_ERR_DO_SAT:
    default:
	break;
    }

    event_add(sat_uninit, o);
}

static void sat_clause_new(sat_t *s)
{
    static sat_t *sat;

    if (!sat)
    {
	sat = s;
	return;
    }

    clause_new(&(sat->clauses));
}

static void sat_clause_new_init(sat_t *s)
{
    sat_clause_new(s);
}

static void sat_clause_new_cb(void)
{
    sat_clause_new(NULL);
}

static int sat_literal_new(sat_t *s, char *name, int val)
{
    static sat_t *sat;

    if (!sat)
    {
	sat = s;
	return 0;
    }

    return clause_add(sat->clauses, name, val, &(sat->variables));
}

static void sat_literal_new_init(sat_t *s)
{
    sat_literal_new(s, NULL, 0);
}

static int sat_literal_new_cb(char *name, int val)
{
    return sat_literal_new(NULL, name, val);
}

static void sat_do(void *o)
{
    sat_t *s = (sat_t *)o;

    s->space.table = s->table;
    s->space.success_cb = sat_success;
    s->space.success_data = s;
    s->space.fail_cb = sat_error;
    s->space.fail_data = s;
    s->errno = SAT_ERR_DO_SAT;

    sp_init(&s->space, s->table, s->print_field, s->print_speed);
}

static void sat_table_create(void *o)
{
    sat_t *s = (sat_t *)o;
    event_func_t next_cb;
    variable_t *var;
    int var_num;

    for (var = s->variables, var_num = 0; var; var = var->next, var_num++);
    if (!(s->table = table_create(s->clauses, var_num)))
    {
	s->errno = SAT_ERR_POS_TABLE;
	next_cb = sat_error;
	goto Exit;
    }

    next_cb = sat_do;

Exit:
    cs_close(s->cs);
    clause_clr(&s->clauses);
    event_add(next_cb, o);
}

static void sat_parse(void *o)
{
    sat_t *s = (sat_t *)o;

    sat_clause_new_init(s);
    sat_literal_new_init(s);

    s->parser_cb.clause_new = sat_clause_new_cb;
    s->parser_cb.literal_new = sat_literal_new_cb,
    s->parser_cb.success_cb = sat_table_create,
    s->parser_cb.success_data = s,
    s->parser_cb.fail_cb = sat_error,
    s->parser_cb.fail_data = s,
    s->errno = SAT_ERR_PARSE;

    parser_init(&(s->parser_cb), s->cs);
}

static sat_print_field_t opt_config_print_field(int opt)
{
    if (opt & FLG(PRINT_CODE))
	return SAT_PRINT_FIELD_CODE;

    if (opt & FLG(PRINT_DIRECTION))
	return SAT_PRINT_FIELD_DIR;

    if (opt & FLG(PRINT_FLAG))
	return SAT_PRINT_FIELD_FLAG;

    if (opt & FLG(PRINT_COLOUR))
	return SAT_PRINT_FIELD_COLOUR;

    if (opt & FLG(PRINT_MONITOR))
	return SAT_PRINT_FIELD_MONITOR;

    /* default */
    return SAT_PRINT_FIELD_NONE;
}

static sat_print_speed_t opt_config_print_speed(int opt)
{
    if (opt & FLG(PRINT_SLOW))
	return SAT_PRINT_SPEED_SLOW;

    if (opt & FLG(PRINT_RAPID))
	return SAT_PRINT_SPEED_FAST;

    /* default */
    return SAT_PRINT_SPEED_MEDIUM;
}

static void sat_usage(char *app_name)
{
    fprintf(stdout, 
	"usage:\n"
	"\n"
	"%s [ OPTIONS ] [-%c] \"<cnf_predicate>\"\n"
	"  or\n"
	"%s [ OPTIONS ] -%c <file_name>\n"
	"  or\n"
	"%s -%c | -%c\n"
	"\n"
	"the predicate must be of the form: (p1 or -p2) and (-p1 or p4 or p3) "
	"and (-p4)\n"
	"where p1, p2, p3, p4, etc... are valid alphanumeric names\n"
	"\n"
	"OPTIONS (in each of the following categories the options are not "
	"compatible with each other):\n"
	"input predicate\n"
	"  -%c: the cnf_predicate is given at command line surrounded by "
	"quotes (default)\n"
	"  -%c: the predicate is read from the specified file: file_name\n"
	"\n"
	"field to print\n"
	"  -%c: do not use graphical display (default)\n"
	"  -%c: display cellular automata's code field\n"
	"  -%c: display cellular automata's direction field\n"
	"  -%c: display cellular automata's flag field\n"
	"  -%c: display cellular automata's colour field\n"
	"  -%c: display cellular automata's monitor field\n"
	"\n"
	"display speed\n"
	"  -%c: slow: %g seconds per iteration\n"
	"  -%c: medium: %g seconds per iteration (default)\n"
	"  -%c: rapid: %g seconds per iteration\n"
	"\n"
	"other options (these options are compatible with each other)\n"
	"  -%c: print this message and exit\n"
	"  -%c: clear screen and enable cursor in case of premature "
	"application termination\n"
	"\n"
	"%c IAS, April 2006\n", 
	app_name, OPT(INPUT_STRING), 
	app_name, OPT(INPUT_FILE), 
	app_name, OPT(HELP), OPT(CURSOR), 
	OPT(INPUT_STRING), OPT(INPUT_FILE), 
	OPT(PRINT_NONE),	
	OPT(PRINT_CODE), 
	OPT(PRINT_DIRECTION), 
	OPT(PRINT_FLAG), 
	OPT(PRINT_COLOUR), 
	OPT(PRINT_MONITOR), 
	OPT(PRINT_SLOW), (double)SPEED_SLOW/MICRO_DEVIDOR, 
	OPT(PRINT_MEDIUM), (double)SPEED_MEDIUM/MICRO_DEVIDOR, 
	OPT(PRINT_RAPID), (double)SPEED_RAPID/MICRO_DEVIDOR, 
	OPT(HELP), OPT(CURSOR),
	ASCII_COPYRIGHT);
}

static void clear_cursor(void)
{
    fprintf(stdout, "%s", CURSOR_ENABLE);
    fflush(stdout);
}

static int opt_get(int argc, char **argv)
{
    int opt_flags = 0;
    char opt;

    sat_args_init();

    while ((opt = getopt(argc, argv, OPT_STRING)) != -1)
    {
	if (opt == OPT(HELP))
	    ASSERT_INPUT(HELP, opt_flags);
	else if (opt == OPT(CURSOR))
	    ASSERT_INPUT(CURSOR, opt_flags);
	else if (opt == OPT(PRINT_NONE))
	    ASSERT_INPUT(PRINT_NONE, opt_flags);
	else if (opt == OPT(PRINT_CODE))
	    ASSERT_INPUT(PRINT_CODE, opt_flags);
	else if (opt == OPT(PRINT_DIRECTION))
	    ASSERT_INPUT(PRINT_DIRECTION, opt_flags);
	else if (opt == OPT(PRINT_FLAG))
	    ASSERT_INPUT(PRINT_FLAG, opt_flags);
	else if (opt == OPT(PRINT_COLOUR))
	    ASSERT_INPUT(PRINT_COLOUR, opt_flags);
	else if (opt == OPT(PRINT_MONITOR))
	    ASSERT_INPUT(PRINT_MONITOR, opt_flags);
	else if (opt == OPT(PRINT_SLOW))
	    ASSERT_INPUT(PRINT_SLOW, opt_flags);
	else if (opt == OPT(PRINT_MEDIUM))
	    ASSERT_INPUT(PRINT_MEDIUM, opt_flags);
	else if (opt == OPT(PRINT_RAPID))
	    ASSERT_INPUT(PRINT_RAPID, opt_flags);
	else if (opt == OPT(INPUT_STRING) || opt == OPT_INPUT_DEFAULT)
	    ASSERT_INPUT_SOURCE(INPUT_STRING, opt_flags);
	else if (opt == OPT(INPUT_FILE))
	    ASSERT_INPUT_SOURCE(INPUT_FILE, opt_flags);
	else
	    return -1;
    }

    return opt_flags ? opt_flags : -1;
}

static int opt_config(char *app_name, int opt_flags)
{
    if (opt_flags == -1)
    {
	fprintf(stderr, "try %s -h for more information\n", app_name);
	return -1;
    }

    if (opt_flags & flags_general)
    {
	if (opt_flags & FLG(HELP))
	    sat_usage(app_name);
	if (opt_flags & FLG(CURSOR))
	    clear_cursor();
    }

    if (opt_flags & flags_input)
    {
	sat.print_field = opt_config_print_field(opt_flags);
	sat.print_speed = opt_config_print_speed(opt_flags);
	event_add(sat_parse, &sat);
    }

    return 0;
}

int sat_init(int argc, char **argv)
{
    return opt_config(argv[0], opt_get(argc, argv));
}

