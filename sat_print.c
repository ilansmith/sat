#include "sat_ca.h"
#include "sat_variable.h"
#include "sat_table.h"
#include "util.h"
#include <stdio.h>
#include <unistd.h>

static void sat_print(void *o);
static int dim, print_pause_microsec ;

static void sat_print_set(void *o)
{
	event_add(sat_print, o);
}

static void print_border_vertical(void)
{
	char colour[FMT_COLOUR_SIZE];
	char field[FMT_COLOUR_SIZE];
	int i;

	sprintf(colour, FMT_COLOUR, ATTR_BRIGHT, COL_WHITE, COL_BCK_BLACK);
	sprintf(field, "%s", colour);
	fprintf(stdout, "%s", field);
	fprintf(stdout, "%s", "+");
	for (i = 0; i < dim; i++)
		fprintf(stdout, "%s", "-");
	fprintf(stdout, "%s", "+");
	sprintf(field, "%s\n", COL_CLEAR);
	fprintf(stdout, "%s", field);
}

static void print_border_horizontal(void)
{
	char colour[FMT_COLOUR_SIZE];
	char field[2 * FMT_COLOUR_SIZE + 2];

	sprintf(colour, FMT_COLOUR, ATTR_BRIGHT, COL_WHITE, COL_BCK_BLACK);
	sprintf(field, "%s%s%s", colour, "|", COL_CLEAR);
	fprintf(stdout, "%s", field);
}

static void sat_print(void *o)
{
	int i;

	fprintf(stdout, FMT_CURSOR_UP, dim + 2);
	print_border_vertical();
	for (i = 0; i < dim; i++) {
		int j;
		char colour[FMT_COLOUR_SIZE];

		print_border_horizontal();
		for (j = 0; j < dim; j++) {
			sat_print_t printer;
			char field[FMT_COLOUR_SIZE + 12];

			sat_print_params_get(o, i, j, &printer);
			sprintf(colour, FMT_COLOUR, printer.is_bright,
				printer.colour, COL_BCK_BLACK);
			sprintf(field, "%s%c%s", colour, printer.representation,
				COL_CLEAR);
			fprintf(stdout, "%s", field);
		}
		print_border_horizontal();
		fprintf(stdout, "\n");
	}
	print_border_vertical();
	fflush(stdout);

	usleep(print_pause_microsec);
	event_add(sat_print_set, o);
}

static void print_speed_set(sat_print_speed_t speed)
{
	switch (speed)
	{
	case SAT_PRINT_SPEED_SLOW:
		print_pause_microsec = SPEED_SLOW;
		break;
	case SAT_PRINT_SPEED_FAST:
		print_pause_microsec = SPEED_RAPID;
		break;
	case SAT_PRINT_SPEED_MEDIUM:
	default:
		print_pause_microsec = SPEED_MEDIUM;
		break;
	}
}

void sat_print_init(void *o, int space_dim, sat_print_speed_t speed)
{
	int i;

	dim = space_dim;
	print_speed_set(speed);
	event_add(sat_print, o);

	/* initiate printing area */
	fprintf(stdout, "%s", CURSOR_DISSABLE);
	fprintf(stdout, "%s", CLEAR_SCREEN);
	for (i = 0; i < dim + 2; i++)
		fprintf(stdout, "\n");
	fflush(stdout);
}

void sat_print_uninit(void)
{
	fprintf(stdout, "%s", CURSOR_ENABLE);
	fflush(stdout);
}

void sat_print_results(table_t *table, variable_t *variables)
{
	int i;

	fprintf(stdout, "expression is %ssatisfyable\n", table->assignment_num ?
		"" : "not ");

	for (i = 0; i < table->assignment_num; i++) {
		int j;

		fprintf(stdout, "assignment %d: ", i + 1);
		for (j = 0; j < table->var_num; j++) {
			fprintf(stdout, "%s=%-7s",
				literal_id2name(variables,
				table->assignments[i][j].id),
				table->assignments[i][j].tv ?
				"TRUE" : "FALSE");
		}
		fprintf(stdout, "\n");
	}
	fflush(stdout);
}

