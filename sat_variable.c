#include "sat_variable.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static variable_t *variable_alloc(char *name, int id)
{
	variable_t *var;

	if (!(var = calloc(1, sizeof(variable_t))))
		return NULL;

	snprintf(var->name, sizeof(var->name), "%s", name);
	var->id = id;
	return var;
}

static void variable_free(variable_t *var)
{
	free(var);
}

int variable_add(variable_t **variables, char *name)
{
	int cnt = 1;

	for ( ; *variables; variables = &(*variables)->next) {
		if (!strcmp((*variables)->name, name))
			return (*variables)->id;

		cnt++;
	}

	return ((*variables) = variable_alloc(name, cnt)) ?
		(*variables)->id : -1;
}

void variables_clr(variable_t **variables)
{
	variable_t *tmp;

	while ((tmp = *variables)) {
		*variables = (*variables)->next;
		variable_free(tmp);
	}
}

char *literal_id2name(variable_t *variables, int id)
{
	for (; variables && variables->id != id; variables = variables->next);

	return variables ? variables->name : "unknown";
}

