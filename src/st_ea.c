/*
 * Extensible Attributes Code
 *
 * Copyright (C) 2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "st_memory.h"
#include "utils.h"
#include "st_ea.h"
#include "st_scanf.h"


int ea_strdup(struct ipam_ea *ea, const char *value)
{
	int len;

	if (value == NULL) {
		ea->len   = 0;
		ea->value = NULL;
		return 1;
	}
	len = strlen(value) + 1;
	ea->value = malloc(len);
	if (ea->value == NULL) {
		ea->len = 0;
		return -1;
	}
	strcpy(ea->value, value);
	ea->len = len;
#ifdef DEBUG_ST_MEMORY
	debug_memory(7, "Allocating %d bytes for EA_value '%s'\n", len, value);
	total_memory += len;
#endif
	return 1;
}

void free_ea_array(struct ipam_ea *ea, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (ea[i].value) {
			debug(MEMORY, 7, "Freeing %d bytes for EA_value '%s'\n",
					ea[i].len, ea[i].value);
			free_ea(&ea[i]);
		}
	}
	st_free(ea, sizeof(struct ipam_ea) * n);
}

struct ipam_ea *alloc_ea_array(int n)
{
	int j;
	struct ipam_ea *ea;

	if (n <= 0) {
		fprintf(stderr, "BUG, %s called with %d size\n", __func__, n);
		return NULL;
	}
	ea = st_malloc_nodebug(n * sizeof(struct ipam_ea), "ipam_ea");
	if (ea == NULL)
		return NULL;

	for (j = 0; j < n; j++) {
		ea[j].name  = NULL;
		ea[j].value = NULL;
		ea[j].len   = 0;
	}
	return ea;
}

struct ipam_ea *realloc_ea_array(struct ipam_ea *ea, int old_n, int new_n)
{
	int j;
	struct ipam_ea *new_ea;

	if (new_n <= old_n) {
		fprintf(stderr, "BUG, %s called new size < old_size\n", __func__);
		return NULL;
	}
	new_ea = st_realloc_nodebug(ea, new_n * sizeof(struct ipam_ea),
			old_n * sizeof(struct ipam_ea),
			"ipam_ea");
	if (new_ea == NULL)
		return NULL;

	for (j = old_n; j < new_n; j++) {
		new_ea[j].name  = NULL;
		new_ea[j].value = NULL;
		new_ea[j].len   = 0;
	}
	return new_ea;
}

int filter_ea(const struct ipam_ea *ea, int ea_nr, const char *ea_name,
		const char *value, char op)
{
	int j, a, b, res, err;
	int found = 0;
	char *s;

	for (j = 0; j < ea_nr; j++) {
		if (!strcmp(ea_name, ea[j].name)) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		debug(FILTER, 1, "Cannot filter on attribute '%s'\n", ea_name);
		return 0;
	}
	debug(FILTER, 5, "We will filter on EA: '%s'\n", ea[j].name);
	s = ea[j].value;
	if (s == NULL) /* EA Value has not been set */
		return 0;
	switch (op) {
	case '=':
		return (!strcmp(s, value));
	case '#':
		return strcmp(s, value);
	case '~':
		res = st_sscanf(s, value);
		return (res < 0 ? 0 : 1);
	case '%':
		res = st_sscanf_ci(s, value);
		return (res < 0 ? 0 : 1);
	case '<':
	case '>':
		b = string2int(value, &err);
		if (err < 0) {
			debug(FILTER, 1, "Cannot interpret Field '%s' as an INT\n", value);
			return -1;
		}
		a = string2int(s, &err);
		/* if Extended Attribute is not and Int we don't return an error,
		 * just no match
		 */
		if (err < 0) {
			debug(FILTER, 4, "Cannot interpret EA '%s' as an INT\n", s);
			return 0;
		}
		if (op == '>')
			return (a > b);
		return (b > a);
	default:
		debug(FILTER, 1, "Unsupported op '%c' for Extended Attribute\n", op);
		return -1;
	}
}
