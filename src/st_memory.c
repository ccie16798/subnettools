/*
 * memory helper functions
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

void *st_malloc(unsigned long n, char *s) {
	void *ptr;

	ptr = malloc(n);
	if (ptr == NULL) {
		if (n > 10 * 1024)
			fprintf(stderr, "Unable to allocate %lu Kbytes for %s\n", n / 1024, s);
		else
			fprintf(stderr, "Unable to allocate %lu bytes for %s\n", n, s);
		return NULL;
	}
	if (n > 10 * 1024) {
		debug(MEMORY, 3, "Allocated %lu Kbytes for %s\n", n / 1024, s);
	} else {
		debug(MEMORY, 3, "Allocated %lu bytes for %s\n", n, s);
	}
	return ptr;
}

void *st_realloc(void *ptr, unsigned long n, char *s) {
	void *new_ptr;

	new_ptr = realloc(ptr,  n);
	if (new_ptr == NULL) {
		if (n > 10 * 1024)
			fprintf(stderr, "Unable to reallocate %lu Kbytes for %s\n", n / 1024, s);
		else
			fprintf(stderr, "Unable to reallocate %lu bytes for %s\n", n, s);
		return  NULL;
	}
	if (n > 10 * 1024) {
		debug(MEMORY, 3, "Reallocated %lu Kbytes for %s\n", n / 1024, s);
	} else {
		debug(MEMORY, 3, "Reallocated %lu bytes for %s\n", n, s);
	}
	return new_ptr;
}
