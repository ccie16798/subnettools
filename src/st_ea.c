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

int ea_size(struct ipam_ea *ea) {
	if (ea->value == NULL)
		return 0;
	return ea->len;
}

int ea_strdup(struct ipam_ea *ea, const char *value) {
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
	total_memory += len;
	ea->len = len;
	return 1;
}

void free_ea_array(struct ipam_ea *ea, int n) {
	int i;

	for (i = 0; i < n; i++) {
		total_memory -= ea_size(&ea[i]);
		free(ea[i].value);
		ea[i].value = NULL;
		ea[i].len   = 0;
	}
	total_memory -= sizeof(struct ipam_ea) * n;
	free(ea);
}

struct ipam_ea *alloc_ea_array(int n) {
	int j;
	struct ipam_ea *ea;

	if (n <= 0) {
		fprintf(stderr, "BUG, alloc_ea_array called with %d size\n", n);
		return NULL;
	}
	ea = st_malloc_nodebug(n * sizeof(struct ipam_ea), "ipam_ea");
	if (ea == NULL)
		return NULL;

	for (j = 0; j < n; j++) {
		ea[j].value = NULL;
		ea[j].len   = 0;
	}
	return ea;
}
