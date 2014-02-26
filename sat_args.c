#include "sat_args.h"

unsigned long flags_general;
unsigned long flags_input;

#define _ARG_TABLE_IDX_DEFINE_
#include "sat_args_defines.h"
#undef _ARG_TABLE_IDX_DEFINE_

sat_opt_t sat_opts[] = {
#define _ARG_TABLE_DECLARE_
#include "sat_args_defines.h"
#undef _ARG_TABLE_DECLARE_
};

void sat_args_init(void)
{
    unsigned long flags_print_fields;
    unsigned long flags_print_speeds;
    unsigned long flags_print;

#define _ARG_TABLE_ENTRIES_INIT_
#include "sat_args_defines.h"
#undef _ARG_TABLE_ENTRIES_INIT_

    flags_general = FLG(HELP) | FLG(CURSOR);
    flags_input = FLG(INPUT_STRING) | FLG(INPUT_FILE);

    flags_print_fields = FLG(PRINT_NONE) | FLG(PRINT_CODE) | 
	FLG(PRINT_DIRECTION) | FLG(PRINT_FLAG) | FLG(PRINT_COLOUR) | 
	FLG(PRINT_MONITOR);
    flags_print_speeds = FLG(PRINT_NONE) | FLG(PRINT_SLOW) | FLG(PRINT_MEDIUM) |
	FLG(PRINT_RAPID);
    flags_print = flags_print_fields | flags_print_speeds;

#define _ARG_TABLE_INCOMPAT_INIT_
#include "sat_args_defines.h"
#undef _ARG_TABLE_INCOMPAT_INIT_
}

