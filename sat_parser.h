#ifndef _SAT_PARSER_H
#define _SAT_PARSER_H

#include "event.h"
#include "char_stream.h"

typedef struct parser_cb_t {
	void (*clause_new)(void);
	int (*literal_new)(char *name, int is_negation);
	event_func_t success_cb;
	void *success_data;
	event_func_t fail_cb;
	void *fail_data;
} parser_cb_t;

void parser_init(parser_cb_t *cb, cs_t *cs);
void parser_uninit(parser_cb_t *cb);

#endif

