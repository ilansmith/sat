#include "sat_args_macros.h"

ARG_START
	ARG_ENTRY(HELP,'h', flags_print | flags_input)
	ARG_ENTRY(CURSOR, 'e', flags_print | flags_input)
	ARG_ENTRY(PRINT_NONE, 'n', flags_general | flags_print_speeds |
		flags_print_fields)
	ARG_ENTRY(PRINT_CODE, 'o', flags_general | flags_print_fields)
	ARG_ENTRY(PRINT_DIRECTION, 'd', flags_general | flags_print_fields)
	ARG_ENTRY(PRINT_FLAG, 'l', flags_general | flags_print_fields)
	ARG_ENTRY(PRINT_COLOUR, 'c', flags_general | flags_print_fields)
	ARG_ENTRY(PRINT_MONITOR, 't', flags_general | flags_print_fields)
	ARG_ENTRY(PRINT_SLOW, 's', flags_general | flags_print_speeds)
	ARG_ENTRY(PRINT_MEDIUM, 'm', flags_general | flags_print_speeds)
	ARG_ENTRY(PRINT_RAPID, 'r', flags_general | flags_print_speeds)
	ARG_ENTRY(INPUT_STRING, 'p', flags_general | FLG(INPUT_FILE))
	ARG_ENTRY(INPUT_FILE, 'f', flags_general | FLG(INPUT_STRING))
	ARG_ENTRY(INPUT_MAX, 0, 0)
ARG_END

