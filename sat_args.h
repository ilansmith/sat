#ifndef _SAT_ARGS_H_
#define _SAT_ARGS_H_

#define IDX(arg) IDX_ ## arg
#define OPT(arg) sat_opts[IDX(arg)].opt
#define FLG(arg) sat_opts[IDX(arg)].flag
#define ICP(arg) sat_opts[IDX(arg)].incompat
#define OPT_INPUT_DEFAULT 1
#define OPT_STRING "-heocdltnsmrp:f:"

typedef struct sat_opt_t {
    char opt;
    unsigned long flag;
    unsigned long incompat;
} sat_opt_t;

#define _ARG_TABLE_IDX_DECLARE_
#include "sat_args_defines.h"
#undef _ARG_TABLE_IDX_DECLARE_

extern unsigned long flags_general;
extern unsigned long flags_input;
extern sat_opt_t sat_opts[];

void sat_args_init(void);
#endif
