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

unsigned long total_memory = 0;

void *__st_malloc_nodebug(unsigned long n, const char *s,
		const char *file, const char *func, int line)
{
	void *ptr;

	ptr = malloc(n);
	if (ptr == NULL) {
		if (n > 10 * 1024 * 1024)
			fprintf(stderr, "%s:%s line %d Unable to allocate %lu Mbytes for %s\n",
					file, func, line, n / (1024 * 1024),  s);
		else if (n > 10 * 1024)
			fprintf(stderr, "%s:%s line %d Unable to allocate %lu Kbytes for %s\n",
					file, func, line, n / (1024),  s);
		else
			fprintf(stderr, "%s:%s line %d Unable to allocate %lu bytes for %s\n",
					file, func, line, n,  s);
	}
	total_memory += n;
	return ptr;
}

void *__st_malloc(unsigned long n, const char *s,
		const char *file, const char *func, int line)
{
	void *ptr;

	ptr = malloc(n);
	if (ptr == NULL) {
		if (n > 10 * 1024 * 1024)
			fprintf(stderr, "%s:%s line %d Unable to allocate %lu Mbytes for %s\n",
					file, func, line, n / (1024 * 1024), s);
		else if (n > 10 * 1024)
			fprintf(stderr, "%s:%s line %d Unable to allocate %lu Kbytes for %s\n",
					file, func, line, n / (1024), s);
		else
			fprintf(stderr, "%s:%s line %d Unable to allocate %lu bytes for %s\n",
					file, func, line, n, s);
		return NULL;
	}
	total_memory += n;
	if (n > 10 * 1024 * 1024) {
		debug_memory(3, "%s:%s line %d Allocated %lu Mbytes for %s\n",
				file, func, line, n / (1024 * 1024), s);
	} else if (n > 10 * 1024) {
		debug_memory(3, "%s:%s line %d Allocated %lu Kbytes for %s\n",
				file, func, line, n / (1024), s);
	} else {
		debug_memory(3, "%s:%s line %d Allocated %lu bytes for %s\n",
				file, func, line, n, s);
	}
	return ptr;
}

void *__st_realloc(void *ptr, unsigned long new, unsigned long old, const char *s,
		const char *file, const char *func, int line)
{
	void *new_ptr;

	new_ptr = realloc(ptr,  new);
	if (new_ptr == NULL) {
		if (new > 10 * 1024 * 1024)
			fprintf(stderr, "%s:%s line %d Unable to reallocate %lu Mbytes for %s\n",
					 file, func, line, new / (1024 * 1024), s);
		else if (new > 10 * 1024)
			fprintf(stderr, "%s:%s line %d Unable to reallocate %lu Kbytes for %s\n",
					 file, func, line, new / (1024) , s);
		else
			fprintf(stderr, "%s:%s line %d Unable to reallocate %lu bytes for %s\n",
					 file, func, line, new, s);
		return  NULL;
	}
	total_memory += (new - old);
	if (new > 10 * 1024 * 1024) {
		debug_memory(3, "%s:%s line %d Reallocated %lu Mbytes for %s\n",
					 file, func, line, new / (1024 * 1024), s);
	} else if (new > 10 * 1024) {
		debug_memory(3, "%s:%s line %d Reallocated %lu Kbytes for %s\n",
					 file, func, line, new / 1024, s);
	} else {
		debug_memory(3, "%s:%s line %d Reallocated %lu bytes for %s\n",
					 file, func, line, new, s);
	}
	return new_ptr;
}

void *__st_realloc_nodebug(void *ptr, unsigned long new, unsigned long old, const char *s,
		const char *file, const char *func, int line)
{
	void *new_ptr;

	new_ptr = realloc(ptr,  new);
	if (new_ptr == NULL) {
		if (new > 10 * 1024 * 1024)
			fprintf(stderr, "%s:%s line %d Unable to reallocate %lu Mbytes for %s\n",
					 file, func, line, new / (1024 * 1024), s);
		else if (new > 10 * 1024)
			fprintf(stderr, "%s:%s line %d Unable to reallocate %lu Kbytes for %s\n",
					 file, func, line, new / (1024) , s);
		else
			fprintf(stderr, "%s:%s line %d Unable to reallocate %lu bytes for %s\n",
					 file, func, line, new, s);
		return  NULL;
	}
	total_memory += (new - old);
	return new_ptr;
}

char *st_strdup(const char *s)
{
	char *broumf;
	int n;

	if (s == NULL)
		return NULL;
	n = strlen(s) + 1;
	broumf = malloc(n);
	if (broumf == NULL) {
		fprintf(stderr, "Unable to allocate %d bytes for '%s'\n", n, s);
		return NULL;
	}
	total_memory += n;
	strcpy(broumf, s);
	debug(MEMORY, 5, "Allocated %d bytes for '%s'\n", n, s);
	return broumf;
}

void st_free_string(char *s)
{
	if (s == NULL)
		return;
	total_memory -= (strlen(s) + 1);
	debug(MEMORY, 6, "Freeing string '%s', %d bytes\n", s, (int)(strlen(s) + 1));
	free(s);
}

void st_free(void *ptr, unsigned long len)
{
	if (ptr == NULL)
		return;
	total_memory -= len;
	debug(MEMORY, 6, "Freeing %lu bytes\n", len);
	free(ptr);
}
