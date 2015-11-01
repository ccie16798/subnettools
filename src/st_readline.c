/*
 * optimized FILE format/reader to read lines from from a file with a MAX line length
 * bytes over line limit are discarded
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
#include <fcntl.h>
#include <unistd.h>
#include "st_readline.h"

//#define DEBUG_READ
#ifdef DEBUG_READ
int read_debug_level = 5;
#define debug_read(level, FMT...) \
	do { \
		if (level <= read_debug_level) \
			fprintf(stderr, FMT); \
	} while (0)
#else
#define debug_read(level, FMT...)
#endif

struct st_file *st_open(const char *name, int buffer_size)
{
	int a;
	struct st_file *f;

	a = open(name, O_RDONLY);
	if (a < 0)
		return NULL;
	f = malloc(sizeof(struct st_file));
	if (f == NULL) {
		close(a);
		return NULL;
	}
	if (buffer_size < 1024)
		buffer_size = 1024;
	f->buffer = malloc(buffer_size);
	if (f->buffer == NULL) {
		free(f);
		close(a);
		return NULL;
	}
	f->buffer_size  = buffer_size;
	f->endoffile    = 0;
	f->need_discard = 0;
	f->bp           = f->buffer;
	f->fileno       = a;
	f->bytes        = 0;
	return f;
}

void st_close(struct st_file *f)
{
	close(f->fileno);
	free(f->buffer);
	free(f);
}

/* refill: refill a struct file internal buffer
 * @f  : a pointer to a struct file
 * returns:
 *	number of char read on success
 *	0 on EOF
 *	-1 on error
 */
static int refill(struct st_file *f, char *buff)
{
	int i, size;

	if (f->bp - f->buffer + f->bytes > f->buffer_size / 2) { /* buffer has gone too big */
		debug_read(3, "Moving#%s %d bytes of data\n", buff, f->bytes);
		memcpy(f->buffer, f->bp, f->bytes);
		f->bp = f->buffer;
	}
	size = f->buffer_size / 2;
	i = read(f->fileno, f->bp + f->bytes, size);
	if (i < 0)
		return i;
	if (i == 0) {
		debug_read(5, "No more: bytes: %d i=%d\n", f->bytes, i);
		f->endoffile = 1;
	}
	f->bytes += i;
	debug_read(5, "refill bytes: %d, f->bytes=%d\n", i, f->bytes);
	return i;
}

static void discard_bytes(struct st_file *f)
{
	int len, i;
	char *t;
	int discarded = 0;

	while (1) {
		/* refill buffer until we find a newline */
		debug_read(5, "discarding need refill %d\n", f->bytes);
		i = refill(f, NULL);
		if (i < 0) /* IO/error */
			return;
		t = memchr(f->bp, '\n', f->bytes);
		if (t != NULL || f->endoffile) {
			len = t - f->bp;
			discarded += len;
			debug_read(5, "Trunc discarding %d chars\n", discarded);
			f->bytes  -= (len + 1);
			f->bp += (len + 1);
			break;
		}
		discarded += f->bytes;
		f->bp     += f->bytes;
		f->bytes = 0;
	}
}

char *st_getline_truncate(struct st_file *f, size_t size, int *read, int *discarded)
{
	int i, len;
	char *t, *p;

	if (size < 2)
		return NULL;
	size--; /* for NUL char */
	if (f->need_discard) {
		discard_bytes(f);
		f->need_discard = 0;
	}
	p = f->bp;
	t = memchr(p, '\n', f->bytes);
	/* if we found a newline within 'size' char lets return */
	if (t != NULL) {
		debug_read(8, "GOOD1: len = %d bytes=%d\n", t - f->bp, f->bytes);
		len = t - p;
		if (len <= size) {
			*t  = '\0';
			len++;
			f->bytes  -= len;
			f->bp     += len;
			*discarded = 0;
			*read = len;
			return p;
		}
		/* line too long, discarding bytes */
		p[size]    = '\0';
		len++;
		f->bytes  -= len;
		f->bp     += len;
		*discarded = len - size;
		*read = size + 1;
		debug_read(8, "Trunc1 discarding %d chars\n", *discarded);
		return p;
	}
	/* we are sure there is no newline for up to f->bytes */
	if (f->bytes <= size) { /** need to refill **/
		i = refill(f, "tst");
		if (i < 0)
			return NULL;
		if (i == 0)
			size = f->bytes;
	} else {
		/* no newline found for a FBB (fucking Big Buffer), suspicious */
		p[size]    = '\0';
		debug_read(3, "Need discard#1 : bytes=%d, size=%d\n", f->bytes, size);
		size++;
		f->bp     += size;
		f->bytes  -= size;
		*discarded = f->bytes + 1; /* at least */
		f->need_discard++;
		*read = size;
		return p;
	}
	/* f->bytes >= size here */
	p = f->bp;
	t = memchr(p, '\n', f->bytes);
	if (t != NULL) {
		len = t - p;
		if (len <= size) {
			*t  = '\0';
			len++;
			f->bytes  -= len;
			f->bp     += len;
			*discarded = 0;
			*read = len;
			return p;
		}
		/* line too long, discarding bytes */
		p[size]    = '\0';
		len++;
		f->bytes  -= len;
		f->bp     += len;
		*discarded = len - size;
		*read = size + 1;
		debug_read(5, "Trunc2 discarding %d chars\n", *discarded);
		return p;
	}
	p[size]    = '\0';
	f->bp     += size;
	f->bytes  -= size;
	*read = size + 1;
	*discarded = 0;
	if (f->endoffile && size == 0) {
		*read = 0;
		return NULL;
	}
	if (f->endoffile)
		return p;
	debug_read(3, "Need discard#2 : %d\n", f->bytes);
	f->need_discard++;
	*read = size + 1;
	*discarded = f->bytes; /*at least */
	return p;
}
#ifdef TEST_READ
int main(int argc, char **argv)
{
	int f, i, line_l = 64;
	struct st_file *sf;
	char *s;

	sf = st_open(argv[1], 2048);
	if (sf == NULL)
		exit(1);
	if (argc >= 3)
		line_l = atoi(argv[2]);
	while ((s = st_getline_truncate(sf, line_l, &i, &f))) {
		printf("%s\n", s);
		if (strlen(s) != i - 1)
			fprintf(stderr, "BUG, st_get_line for %s return value=%d, len=%d\n",
				s, i, (int)strlen(s));
	}
	st_close(sf);
}
#endif
