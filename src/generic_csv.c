/*
 *  Generic CSV parser functions
 *
 * Copyright (C) 2014, 2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "debug.h"
#include "heap.h"
#include "generic_csv.h"
#include "utils.h"
#include "st_memory.h"

static int read_csv_header(const char *buffer, struct csv_file *cf)
{
	int i, j, found;
	char *s, *save_s, *s2;
	int pos = 1;
	int bad_header = 0;
	int max_mandatory_pos = 1;
	int no_header = 0;
	char buffer2[CSV_MAX_LINE_LEN];

	i = strxcpy(buffer2, buffer, sizeof(buffer2));
	if (i >= sizeof(buffer2)) {
		fprintf(stderr, "CSV header is too large (%d bytes), max %d bytes\n",
			i, (int)sizeof(buffer2));
		return CSV_HEADER_TOOLONG;
	}
	s = buffer2;
	debug(CSVHEADER, 3, "Trying to parse header in %s\n", cf->file_name);
	debug(CSVHEADER, 5, "CSV header : '%s'\n", s);

	if (cf->is_header == NULL || cf->is_header(s)) { /* check if valid header */
		s = cf->csv_strtok_r(s, cf->delim, &save_s);
		while (s) {
			debug(CSVHEADER, 8, "parsing token '%s' at pos %d\n", s, pos);
			found = 0;
			for (i = 0; ; i++) {
				if (cf->csv_field[i].name == NULL)
					break;
				if (!cf->header_field_compare(s, cf->csv_field[i].name)) {
					cf->csv_field[i].pos = pos;
					found = 1;
					debug(CSVHEADER, 3, "found header field '%s' at pos %d\n", s, pos);
					break;
				}
			}
			if (found == 0) {
				if (cf->default_handler) {
					debug(CSVHEADER, 3, "using default handler for field '%s' at pos %d\n",
							s, pos);
					s2 = st_strdup(s);
					if (s2 == NULL)
						return CSV_ENOMEM;
					i = register_csv_field(cf, s2, 0, 0,
							cf->default_handler);
					cf->csv_field[i].pos = pos;
					cf->csv_field[i].dyn_alloc = 1;
				} else {
					debug(CSVHEADER, 3, "no handler for field '%s' at pos %d\n",
							s, pos);
				}
			}
			pos++;
			s = cf->csv_strtok_r(NULL, cf->delim, &save_s);
		}
		debug(CSVHEADER, 3, "found %d fields\n", pos - 1);
		cf->num_fields = pos - 1;
	} else  {
		debug(CSVHEADER, 2, "File %s does not have a CSV header, using default values\n",
				cf->file_name);
		no_header = 1;
		cf->num_fields = 0;
		/* setting default pos */
		for (i = 0; ; i++) {
			if (cf->csv_field[i].name == NULL)
				break;
			cf->csv_field[i].pos = cf->csv_field[i].default_pos;

			cf->num_fields = max(cf->num_fields, cf->csv_field[i].pos);
			debug(CSVHEADER, 3, "Using default pos %d for field '%s'\n",
					cf->csv_field[i].pos, cf->csv_field[i].name);
		}
	}

	/* check the presence or mandatory fields and set default values */
	for (i = 0; ; i++) {
		if (cf->csv_field[i].name == NULL)
			break;
		if (cf->csv_field[i].mandatory)
			max_mandatory_pos = max(max_mandatory_pos, cf->csv_field[i].pos);
		if (cf->csv_field[i].pos == 0 && cf->csv_field[i].mandatory) {
			bad_header++;
			debug(CSVHEADER, 1, "mandatory CSV field '%s' not found in %s\n",
					cf->csv_field[i].name, cf->file_name);
		}
	}
	/* check for duplicate field pos
	 * this can happen only if NO header and programming error */
	 for (i = 0; ; i++) {
                if (cf->csv_field[i].name == NULL)
			break;
		for (j = 0; ; j++) {
			if (cf->csv_field[j].name == NULL)
				break;
			if (i == j)
				continue;
			if (cf->csv_field[i].pos == cf->csv_field[j].pos && cf->csv_field[j].pos ) {
				bad_header++;
				debug(CSVHEADER, 1, "pos for %s == pos for %s : %d\n",
						cf->csv_field[i].name, cf->csv_field[j].name,
						cf->csv_field[i].pos);
			}
		}
	}
	if (bad_header) {
		fprintf(stderr, "file %s doesn't have a valid CSV header\n", cf->file_name);
		return CSV_BAD_HEADER;
	}
	cf->max_mandatory_pos = max_mandatory_pos;
	if (no_header == 1)
		return CSV_NO_HEADER;
	else
		return CSV_HEADER_FOUND;
}

/*
 * the CSV Body engine
 * it is a private function
 * @f 	  : the CSV file to parse
 * @cf	  : a struct csv_file describing the fields
 * @state : a CSV state; can be set before
 * @date  : a generic structure where you will store the data
 * @init_buffer : if non-NULL, will contain the first line of the file
 * returns :
 * 	>0 on success
 * 	<0 on ERROR
 */
static int read_csv_body(FILE *f, struct csv_file *cf,
		struct csv_state *state, void *data,
		char *init_buffer)
{
	char buffer[CSV_MAX_LINE_LEN];
	struct csv_field *csv_field;
	int i, res;
	char *s, *save_s;
	int pos;
	unsigned long badlines = 0;

	debug_timing_start(2);
	if (init_buffer) {
		s = init_buffer;
		res = 0;
	} else {
		s = fgets_truncate_buffer(buffer, CSV_MAX_LINE_LEN, f, &res);
		if (s == NULL) {
			debug(LOAD_CSV, 1, "File %s doesn't have any content\n", cf->file_name);
			return  CSV_EMPTY_FILE;
		}
	}
	do {
		if (state->line == ULONG_MAX) { /* paranoid check */
			debug(LOAD_CSV, 1, "File %s is too big, we reached ULONG_MAX (%lu)\n",
					cf->file_name, ULONG_MAX);
			return CSV_FILE_MAX_SIZE;
		}
		state->line++;
		if (res) { /* BFB; BIG FUCKING BUFFER; try to handle that  */
			debug(LOAD_CSV, 1, "File %s line %lu is longer than max size %d, discarding %d chars\n",
					cf->file_name, state->line, (int)sizeof(buffer), res);
		}
		debug(LOAD_CSV, 5, "Parsing line %lu : %s \n", state->line, s);
		s = cf->csv_strtok_r(s, cf->delim, &save_s);
		pos  = 0;
		state->badline = 0;
		while (s) {
			pos++;
			if (pos > cf->num_fields) {
				debug(LOAD_CSV, 1, "Line %lu too many tokens \n", state->line);
				break;
			}
			csv_field    = NULL;
			debug(LOAD_CSV, 5, "Parsing token '%s' pos %d \n", s, pos);
			/* try to find the handler */
			for (i = 0; ; i++) {
				debug(LOAD_CSV, 9, "Parsing field pos %d %d : %s\n",
						pos, cf->csv_field[i].pos, cf->csv_field[i].name);
				if (cf->csv_field[i].name == NULL)
					break;
				if (pos == cf->csv_field[i].pos) {
					csv_field = &cf->csv_field[i];
					state->csv_field = csv_field->name;
					debug(LOAD_CSV, 5, "found field handler#%d='%s' pos=%d data : '%s'\n",
							i, csv_field->name, pos, s);
					break;
				}
			}
			if (csv_field && csv_field->handle) {
				res = csv_field->handle(s, data, state);
				if (res == CSV_INVALID_FIELD_BREAK) {
					debug(LOAD_CSV, 2, "Field '%s' data '%s' handler returned %s\n",
							csv_field->name, s, "CSV_INVALID_FIELD_BREAK");
					state->badline = 1;
					break;
				} else if (res == CSV_VALID_FIELD_BREAK) {
					/* found a valid field, but caller told us
					 * nothing interesting on this line */
					debug(LOAD_CSV, 5, "Field '%s' data '%s' handler returned %s\n",
							csv_field->name, s, "CSV_VALID_FIELD_BREAK");
					break;
				} else if (res == CSV_VALID_FIELD_SKIP) {
					debug(LOAD_CSV, 5, "Field '%s' told us to skip %d fields\n",
							csv_field->name, state->skip);
					for (i = 0; i < state->skip && s != NULL; i++) {
						s = cf->csv_strtok_r(NULL, cf->delim, &save_s);
						debug(LOAD_CSV, 6, "Skipping %s\n", s);
					}
					if (s == NULL)
						break;
				} else if (res == CSV_CATASTROPHIC_FAILURE) {/* FATAL ERROR like no more memory*/
					debug(LOAD_CSV, 1,  "line %lu : fatal error, aborting\n", state->line);
					debug_timing_end(2);
					return -2;
				}
			} /* if csv_>field */
			s = cf->csv_strtok_r(NULL, cf->delim, &save_s);
		} /* while s */
		if (pos < cf->max_mandatory_pos) {
			state->badline++;
			debug(LOAD_CSV, 2, "Parsing line %lu, not enough fields : %d, requires : %d\n",
					state->line, pos, cf->max_mandatory_pos);
		}

		if (cf->endofline_callback) {
			res = cf->endofline_callback(state, data);
			if (res == CSV_CATASTROPHIC_FAILURE) {/* FATAL ERROR like no more memory*/
				debug(LOAD_CSV, 1,  "line %lu : fatal error, aborting\n", state->line);
				debug_timing_end(2);
				return -2;
			} else if (res == CSV_END_FILE) {
				debug(LOAD_CSV, 4, "line %lu : endofline callback told us to stop\n",
						state->line);
				break;
			}
		}
		if (state->badline)
			badlines++;
	} while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res)) != NULL);

	/* end of file */
	if (cf->endoffile_callback)
		res = cf->endoffile_callback(state, data);
	else
		res = CSV_VALID_FILE;
	debug(LOAD_CSV, 3, "Parsed %lu lines, %lu good, %lu bad\n",
			state->line, state->line - badlines, badlines);
	debug_timing_end(2);
	return res;
}

static void free_csv_field(struct csv_field *cf)
{
	int i;

	for (i = 0; ; i++) {
		if (cf[i].name == NULL)
			return;
		if (cf[i].dyn_alloc)
			st_free_string(cf[i].name);
	}
}

int generic_load_csv(char *filename, struct csv_file *cf, struct csv_state* state, void *data)
{
	FILE *f;
	char buffer[CSV_MAX_LINE_LEN];
	char *s;
	int res, res2 = 0;

	if (cf->csv_strtok_r == NULL) {
		fprintf(stderr, "coding error:  no strtok function provided\n");
		return -2;
	}
	if (filename == NULL)
		f = stdin;
	else {
		f = fopen(filename, "r");
		if (f == NULL) {
			fprintf(stderr, "cannot open %s for reading\n", filename);
			return CSV_CANNOT_OPEN_FILE;
		}
	}
	s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res);
	if (s == NULL) {
		fprintf(stderr, "empty file %s\n", filename ? filename : "<stdin>");
		fclose(f);
		return CSV_EMPTY_FILE;
        } else if (res) {
		fprintf(stderr, "%s CSV header is longer than the allowed size %d\n",
			 (filename ? filename : "<stdin>"), (int)sizeof(buffer));
		fclose(f);
		return CSV_HEADER_TOOLONG;
	}
	res = read_csv_header(buffer, cf);
	if (res < 0) {
		free_csv_field(cf->csv_field);
		fclose(f);
		return res;
	}
	if (cf->validate_header) {
		res2 = cf->validate_header(cf, data);
		if (res2 < 0) {
			fclose(f);
			free_csv_field(cf->csv_field);
			return res2;
		}
	}
	state->line = 0;
	if (res == CSV_HEADER_FOUND) /* first line was a header, no need to put initial_buff */
		res = read_csv_body(f, cf, state, data, NULL);
	else if (res == CSV_NO_HEADER) /* we need to pass initial buff */
		res = read_csv_body(f, cf, state, data, buffer);
	else {
		fprintf(stderr, "BUG at %s line %d, invalid res=%d\n", __FILE__, __LINE__, res);
		res = -3;
	}
	free_csv_field(cf->csv_field);
	fclose(f);
	return res;
}

/* strcmp could be inlined so we need this */
int generic_header_cmp(const char *s1, const char *s2)
{
	return strcmp(s1, s2);
}

void init_csv_file(struct csv_file *cf, char *file_name, struct csv_field *csv_field,
		char *delim, char * (*func)(char *s, const char *delim, char **save_ptr))
{
	if (cf == NULL)
		return;
	/* mandatory fields */
	cf->csv_field	 = csv_field;
	cf->delim	 = delim;
	cf->csv_strtok_r = func;
	cf->file_name 	 = (file_name ? file_name : "<stdin>");
	/* optional fields */
	cf->is_header		 = NULL;
	cf->validate_header	 = NULL;
	cf->default_handler	 = NULL;
	cf->endofline_callback	 = NULL;
	cf->endoffile_callback	 = NULL;
	cf->header_field_compare = generic_header_cmp;
	cf->num_fields_registered= 0;
}

void init_csv_state(struct csv_state *cs, char *file_name)
{
	memset(cs, 0, sizeof(struct csv_state));
	cs->file_name = (file_name ? file_name : "<stdin>");
}

int register_csv_field(struct csv_file *csv_file, char *name, int mandatory, int default_pos,
		int (*handle)(char *token, void *data, struct csv_state *state))
{
	int i = csv_file->num_fields_registered;
	struct csv_field *cf = csv_file->csv_field;

	cf[i].name        = name;
	cf[i].handle      = handle;
	cf[i].mandatory   = mandatory;
	cf[i].pos 	  = 0;
	cf[i].dyn_alloc	  = 0;
	cf[i].default_pos = default_pos;
	cf[i + 1].name 	  = NULL;
	csv_file->num_fields_registered++;
	debug(CSVHEADER, 3, "Registering handler #%d '%s'\n", i, name);
	return i;
}
