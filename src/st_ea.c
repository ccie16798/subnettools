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

