#include "sat.h"
#include "sat_parser.h"
#include "char_stream.h"
#include <stdio.h>
#include <string.h>

/* quantifiers */
#define AND "and"
#define OR "or"
#define MAX_QUANTIFIER_LEN 3

#define IS_SPACE(c) ((c) == ' ')
#define IS_TAB(c) ((c) == '\t')
#define IS_LPAREN(c) ((c) == '(')
#define IS_RPAREN(c) ((c) == ')')
#define IS_NEGATION(c) ((c) == '-')
#define IS_UNDERSCORE(c) ((c) == '_')
#define IS_ALPHA(c) (('a' <= (c) && (c) <= 'z') || ('A' <= (c) && (c) <= 'Z'))
#define IS_NUMERIC(c) (('0' <= (c) && (c) <= '9'))
#define IS_NEWLINE(c) ((c) == '\n')
#define IS_EOF(c) ((c) == EOF)
#define IS_ALPHANUMERIC(c) (IS_ALPHA(c) || IS_NUMERIC(c))

#define C_NORMAL "\033[00;00m"
#define C_HIGHLIGHT "\033[01;41m"
#define C_BLINK "\033[00;05m"

#define ARRAY_SZ(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef enum parse_errno_t {
	ERR_EARLY_EOF = 0,
	ERR_LPAREN = 1,
	ERR_NON_ADJACENT_NEGATION = 2,
	ERR_VARIABLE_PREFIX = 3,
	ERR_VARIABLE_ILLEGAL = 4,
	ERR_NO_OR = 5,
	ERR_OR_WS = 6,
	ERR_NO_AND = 7,
	ERR_AND_WS = 8,
	ERR_RPAREN = 9,
	ERR_RESERVED_WORDS = 10,
	ERR_MEM_ALLOC = 11,
} parse_errno_t;

typedef struct parser_t {
	parser_cb_t *cb;
	cs_t *cs;
	long error_pos;
	int error_no;
} parser_t;

static parser_t parser;
static void st_clause_start(void *o);
static void st_literal_pre(void *o);

static int is_variable_prefix(char c)
{
	return IS_UNDERSCORE(c) || IS_ALPHA(c);
}

static int is_variable_char(char c)
{
	return IS_UNDERSCORE(c) || IS_ALPHANUMERIC(c);
}

static int is_whitespace(char c)
{
	return IS_SPACE(c) || IS_TAB(c);
}

static int is_variable_delim(char c)
{
	return IS_RPAREN(c) || is_whitespace(c);
}

static int is_reserved_word(char *s)
{
	return !strcasecmp(s, OR) || !strcasecmp(s, AND);
}

static int is_predicate_end(char c)
{
	return IS_NEWLINE(c) || IS_EOF(c);
}

static int eat_whitespace(cs_t *cs)
{
	char c;
	int eaten = 0;

	while (is_whitespace(c = cs_getc(cs)))
		eaten = 1;
	cs_ungetc(cs);

	return eaten;
}

static void clause_new(parser_cb_t *cb)
{
	if (cb->clause_new)
		cb->clause_new();
}

static int literal_new(parser_cb_t *cb, char *name, int val)
{
	return cb->literal_new ? cb->literal_new(name, val) : 0;
}

static int is_quantifier(cs_t *cs, char *quantifier)
{
	char qfr[MAX_QUANTIFIER_LEN + 1], *qptr;
	int len;

	sprintf(qfr, "%s", quantifier);
	qptr = qfr;
	len = strlen(qfr);
	while (qptr < qfr + len && (cs_getc(cs)) == *qptr++);

	return qptr == qfr + len;
}

static int is_quantifier_and(cs_t *cs)
{
	return is_quantifier(cs, AND);
}

static int is_quantifier_or(cs_t *cs)
{
	return is_quantifier(cs, OR);
}

static void st_error(void *o)
{
	parser_t *p = (parser_t *)o;
	cs_t *cs = p->cs;
	char c, *err_msg, str[100] = {0};

	switch(p->error_no)
	{
	case ERR_EARLY_EOF:
		p->error_pos = -1;
		snprintf(str, ARRAY_SZ(str), "%s%s%s%s", C_BLINK, C_HIGHLIGHT,
			"_", C_NORMAL);
		err_msg = "expression terminated prematurely";
		break;
	case ERR_LPAREN:
		err_msg = "a clause must start with a '('";
		break;
	case ERR_NON_ADJACENT_NEGATION:
		err_msg = "'-' must be adjacent to the variable";
		break;
	case ERR_VARIABLE_PREFIX:
		err_msg = "variable prefix can be '_', ['a'-'z'] or ['A'-'Z']";
		break;
	case ERR_VARIABLE_ILLEGAL:
		err_msg = "variables consist of alpha numeric characters only";
		break;
	case ERR_NO_OR:
		err_msg = "literals in a clause must be separated by an \"or\"";
		break;
	case ERR_OR_WS:
		err_msg = "literals are separated by an \"or\" surrounded by "
			"whitespaces";
		break;
	case ERR_NO_AND:
		err_msg = "clauses are separated by an \"and\"";
		break;
	case ERR_AND_WS:
		err_msg = "clauses are separated by an \"and\" surrounded by "
			"whitespaces";
		break;
	case ERR_RPAREN:
		err_msg = "')' is either end of predicate or is followed by "
			"an \"and\" surrounded by white spaces and followed by "
			"another clause";
		break;
	case ERR_RESERVED_WORDS:
		err_msg = "\"or\" and \"and\" are reserved words";
		break;
	case ERR_MEM_ALLOC:
		err_msg = "could not allocate memory for new literal";
		break;
	default:
		/* should never get here */
		err_msg = "";
		break;
	}

	cs_rewind(cs);
	printf("parse error: %s\nCNF expression: ", err_msg);
	while (!is_predicate_end(c = cs_getc(cs))) {
		char *ch_prefix = cs_getpos(cs) - 1 == p->error_pos ?
			C_HIGHLIGHT : C_NORMAL;

		printf("%s%c%s", ch_prefix, c, C_NORMAL);
	}
	printf("%s\n", str);

	event_add(p->cb->fail_cb, p->cb->fail_data);
}

static void st_clause_end(void *o)
{
	parser_t *p = (parser_t *)o;
	cs_t *cs = p->cs;
	char c;
	event_func_t next_cb;
	void *next_data;
	int eaten;

	eaten = eat_whitespace(cs);
	if (is_predicate_end(cs_getc(cs))) {
		/* successful parse completion */
		next_cb = p->cb->success_cb;
		next_data = p->cb->success_data;
		goto Exit;
	}

	cs_ungetc(cs);
	next_data = o;
	p->error_pos = cs_getpos(cs);

	if (!eaten) {
		p->error_no = ERR_RPAREN;
		next_cb = st_error;
		goto Exit;
	}

	if (!is_quantifier_and(cs)) {
		p->error_no = ERR_NO_AND;
		next_cb = st_error;
		goto Exit;
	}

	if (!is_whitespace(c = cs_getc(cs))) {
		p->error_pos = cs_getpos(cs) - 1;

		if (is_predicate_end(c))
			p->error_no = ERR_EARLY_EOF;
		else if (IS_LPAREN(c))
			p->error_no = ERR_AND_WS;
		else
			p->error_no = ERR_NO_AND;

		next_cb = st_error;
		goto Exit;
	}

	next_cb = st_clause_start;

Exit:
	event_add(next_cb, next_data);
}

static void st_literal_post(void *o)
{
	parser_t *p = (parser_t *)o;
	cs_t *cs = p->cs;
	char c;
	event_func_t next_cb;

	eat_whitespace(cs);
	if (is_predicate_end(c = cs_getc(cs))) {
		p->error_no = ERR_EARLY_EOF;
		next_cb = st_error;
		goto Exit;
	}

	if (IS_RPAREN(c)) {
		next_cb = st_clause_end;
		goto Exit;
	}

	cs_ungetc(cs);
	p->error_pos = cs_getpos(cs);
	if (!is_quantifier_or(cs)) {
		p->error_no = ERR_NO_OR;
		next_cb = st_error;
		goto Exit;
	}

	if (!is_whitespace(c = cs_getc(cs))) {
		p->error_pos = cs_getpos(cs) - 1;
		p->error_no = is_predicate_end(c) ? ERR_EARLY_EOF : ERR_OR_WS;
		next_cb = st_error;
		goto Exit;
	}
	cs_ungetc(cs);
	next_cb = st_literal_pre;

Exit:
	event_add(next_cb, o);
}

static void st_literal(void *o)
{
	parser_t *p = ((parser_t *)o);
	cs_t *cs = p->cs;
	char c, var[MAX_VARIABLE_SZ], *cptr;
	event_func_t next_cb;
	int val = -1;

	p->error_pos = cs_getpos(cs);
	cptr = var;
	while (is_variable_char(c = cs_getc(cs)) || val == -1) {
		if (cptr == var + sizeof(var) - 1) {
			cs_setpos(cs, -1);
			break;
		}
		if (val == -1 && !(val = IS_NEGATION(c) ? 0 : 1))
			continue;
		*cptr = c;
		cptr++;
	}
	*cptr = 0;
	cs_ungetc(cs);

	if (is_predicate_end(c)) {
		p->error_no = ERR_EARLY_EOF;
		next_cb = st_error;
		goto Exit;
	}

	if (!is_variable_delim(c)) {
		p->error_pos = cs_getpos(cs);
		p->error_no = ERR_VARIABLE_ILLEGAL;
		next_cb = st_error;
		goto Exit;
	}

	if (is_reserved_word(var)) {
		p->error_no = ERR_RESERVED_WORDS;
		next_cb = st_error;
		goto Exit;
	}

	if (literal_new(p->cb, var, val)) {
		p->error_no = ERR_MEM_ALLOC;
		next_cb = st_error;
		goto Exit;
	}

	next_cb = st_literal_post;

Exit:
	event_add(next_cb, o);
}

static void st_literal_pre(void *o)
{
	parser_t *p = (parser_t *)o;
	cs_t *cs = p->cs;
	event_func_t next_cb;
	char c;
	int val;

	eat_whitespace(cs);

	if (is_predicate_end(c = cs_getc(cs))) {
		p->error_no = ERR_EARLY_EOF;
		next_cb = st_error;
		goto Exit;
	}

	p->error_pos = cs_getpos(cs) - 1;
	if (!(val = IS_NEGATION(c) ? 0 : 1))
		c = cs_getc(cs);
	cs_ungetc(cs);

	if (!is_variable_prefix(c)) {
		if (is_predicate_end(c)) {
			p->error_no = ERR_EARLY_EOF;
		}
		else if (is_whitespace(c)) {
			p->error_no = ERR_NON_ADJACENT_NEGATION;
		}
		else {
			p->error_no = ERR_VARIABLE_PREFIX;
			if (!val)
				p->error_pos++;
		}

		next_cb = st_error;
		goto Exit;
	}

	if (!val)
		cs_setpos(cs, -1);
	next_cb = st_literal;

Exit:
	event_add(next_cb, o);
}

static void st_clause_start(void *o)
{
	parser_t *p = (parser_t *)o;
	cs_t *cs = p->cs;
	event_func_t next_cb;
	char c;

	clause_new(p->cb);
	eat_whitespace(cs);
	if (!IS_LPAREN(c = cs_getc(cs))) {
		if (is_predicate_end(c)) {
			p->error_no = ERR_EARLY_EOF;
		}
		else {
			p->error_no = ERR_LPAREN;
			p->error_pos = cs_getpos(cs) - 1;
		}
		next_cb = st_error;
		goto Exit;
	}

	next_cb = st_literal_pre;

Exit:
	event_add(next_cb, o);
}

void parser_init(parser_cb_t *cb, cs_t *cs)
{
	parser.cb = cb;
	parser.cs = cs;
	parser.error_pos = EOF;

	event_add(st_clause_start, &parser);
}

