/*
 * Generic CSV parser functions
 *
 * Copyright (C) 2014-2018 Etienne Basset <etienne POINT basset AT ensta POINT org>
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
#include "st_readline.h"

static int read_csv_header(const char *buffer, struct csv_file *cf)
{
	int i, j, found;
	char *s, *save_s;
	int pos	       = 1;
	int bad_header = 0;
	int no_header  = 0;
	int mandatory_fields = 0;
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
		s = cf->csv_strtok_r(s, cf->delim, &save_s,
			       cf->string_delim, cf->string_delim_escape);
		while (s) {
			debug(CSVHEADER, 8, "parsing token '%s' at pos %d\n", s, pos);
			found = 0;
			for (i = 0; ; i++) {
				if (cf->csv_field[i].name == NULL)
					break;
				if (!cf->header_field_compare(s, cf->csv_field[i].name)) {
					cf->csv_field[i].pos = pos;
					found = 1;
					debug(CSVHEADER, 3,
							"found header field '%s' at pos %d\n",
							s, pos);
					break;
				}
			}
			/* dynamic registration of unknow Field names */
			if (found == 0) {
				if (cf->default_handler) {
					debug(CSVHEADER, 3,
							"default handler for field '%s' at pos %d\n",
							s, pos);
					i = register_csv_field(cf, s, 0,
							pos, 0, cf->default_handler);
					if (i == CSV_ENOMEM)
						return CSV_ENOMEM;
				} else {
					debug(CSVHEADER, 3, "no handler for field '%s' at pos %d\n",
							s, pos);
				}
			}
			pos++;
			s = cf->csv_strtok_r(NULL, cf->delim, &save_s,
				       cf->string_delim, cf->string_delim_escape);
		}
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
			if (cf->csv_field[i].default_pos > cf->max_fields) { /* programming error only */
				fprintf(stderr, "BUG, file %s wants a CSV header with a handler at pos %d\n",
						cf->file_name, cf->csv_field[i].default_pos  );
				return CSV_BAD_HEADER;
			}


			cf->csv_field[i].pos = cf->csv_field[i].default_pos;

			cf->num_fields = max(cf->num_fields, cf->csv_field[i].pos);
			debug(CSVHEADER, 3, "Using default pos %d for field '%s'\n",
					cf->csv_field[i].pos, cf->csv_field[i].name);
		}
	}

	debug(CSVHEADER, 3, "found %d fields\n", cf->num_fields);
	/* the CSV field_sorted must be allocated now and not in init_csv_file
	* reason is that we might be interested in only 4 fields in a CSV (input of init_csv_file)
	*  but on a given file one of them could be at position 20
	* csv_field_sorted thus consume MORE memory, but speeds up CSV treatment
	*/
	cf->csv_field_sorted = st_malloc(sizeof(struct csv_field) * (cf->num_fields + 1) , "CSV Field sorted");
	if (cf->csv_field_sorted == NULL) /* unlikely but .. */
		return -1;
	memset(cf->csv_field_sorted, 0, sizeof(struct csv_field) * (cf->num_fields + 1) );
	/* check the presence or mandatory fields and set default values */
	for (i = 0; ; i++) {
		if (cf->csv_field[i].name == NULL)
			break;
		pos = cf->csv_field[i].pos;
		debug(CSVHEADER, 3, "copying field %d at in csv_field_sorted %d\n", i, pos);
		memcpy(&cf->csv_field_sorted[pos],  &cf->csv_field[i], sizeof(cf->csv_field[i]));

		if (cf->csv_field[i].mandatory)
			mandatory_fields++;
		if (cf->csv_field[i].pos == 0 && cf->csv_field[i].mandatory) {
			bad_header++;
			debug(CSVHEADER, 1, "mandatory CSV field '%s' not found in %s\n",
					cf->csv_field[i].name, cf->file_name);
		}
	}
	/* check for duplicate field pos
	 * this can happen only if NO header and programming error
	 */
	for (i = 0; ; i++) {
		if (cf->csv_field[i].name == NULL)
			break;
		for (j = 0; ; j++) {
			if (cf->csv_field[j].name == NULL)
				break;
			if (i == j)
				continue;
			if (cf->csv_field[i].pos == cf->csv_field[j].pos
					&& cf->csv_field[j].pos) {
				bad_header++;
				debug(CSVHEADER, 1, "pos for %s == pos for %s : %d\n",
						cf->csv_field[i].name, cf->csv_field[j].name,
						cf->csv_field[i].pos);
			}
		}
	}
	if (mandatory_fields < cf->mandatory_fields) {
		debug(CSVHEADER, 1, "file %s has only %d mandatory fields, %d required\n",
				cf->file_name, mandatory_fields, cf->mandatory_fields);
		bad_header = 1;
	}
	if (bad_header) {
		fprintf(stderr, "file %s doesn't have a valid CSV header\n", cf->file_name);
		return CSV_BAD_HEADER;
	}
	if (no_header == 1)
		return CSV_NO_HEADER;
	else
		return CSV_HEADER_FOUND;
}

/*
 * the CSV Body engine
 * it is a private function
 * @f	  : the CSV file to parse
 * @cf    : a struct csv_file describing the fields
 * @state : a CSV state; can be set before
 * @data  : a generic structure where you will store the data
 * @init_buffer : if non-NULL, will contain the first line of the file
 * returns:
 *	>0 on success
 *	<0 on ERROR
 */
static int read_csv_body(struct st_file *f, struct csv_file *cf,
		struct csv_state *state, void *data,
		char *init_buffer)
{
	char buffer[CSV_MAX_LINE_LEN];
	struct csv_field *csv_field;
	int i, res;
	char *s, *save_s;
	int pos;
	unsigned long badlines = 0;
	char sd = cf->string_delim;
	char se = cf->string_delim_escape;

	debug_timing_start(2);
	/* get the first line from f or from init_buffer if set */
	if (init_buffer) {
		s = init_buffer;
		res = 0;
	} else {
		s = st_getline_truncate(f, CSV_MAX_LINE_LEN, &i, &res);
		if (s == NULL) {
			debug(LOAD_CSV, 1, "File %s doesn't have any content\n", cf->file_name);
			return  CSV_EMPTY_FILE;
		}
	}
	state->badline = 0;
	do { /* loop for all lines in the files */
		if (state->line >= CSV_MAX_LINE_NUMBER) {
			debug(LOAD_CSV, 1, "File %s has too many lines, MAX=%lu\n",
					cf->file_name, CSV_MAX_LINE_NUMBER);
			return CSV_FILE_MAX_SIZE;
		}
		state->line++;
		if (res) { /* BFB; BIG FUCKING BUFFER; try to handle that  */
			debug(LOAD_CSV, 1, "File %s line %lu longer than %d, discarding %d chars\n",
					cf->file_name, state->line, (int)sizeof(buffer), res);
		}
		debug(LOAD_CSV, 5, "Parsing line %lu : '%s'\n", state->line, s);
		if (cf->startofline_callback) {
			res = cf->startofline_callback(state, data);
			if (res == CSV_CATASTROPHIC_FAILURE) {/* FATAL ERROR like no more memory*/
				debug(LOAD_CSV, 1,  "File %s line %lu : fatal error, aborting\n",
						cf->file_name, state->line);
				debug_timing_end(2);
				return res;
			}
		}
		s = cf->csv_strtok_r(s, cf->delim, &save_s, sd, se);
		pos = 0;
		state->badline = 0;
		state->mandatory_fields = 0;
		while (s) {
			pos++;
			if (pos > cf->num_fields) {
				debug(LOAD_CSV, 1, "File %s line %lu : too many tokens\n",
					       cf->file_name, state->line);
				break;
			}
			csv_field    = NULL;
			debug(LOAD_CSV, 5, "Parsing token '%s' pos %d\n", s, pos);
			csv_field = &cf->csv_field_sorted[pos];
			state->csv_field = csv_field->name;
			debug(LOAD_CSV, 5, "handler='%s' pos=%d data='%s'\n",
							 csv_field->name, pos, s); 
			if (csv_field->handle) {
				res = csv_field->handle(s, data, state);
				if (csv_field->mandatory)
					state->mandatory_fields++;
				if (res == CSV_INVALID_FIELD_BREAK) {
					/* more interesting debug info could be found in the actual handler
					 * so debug level should be higher than 3
					 */
					debug(LOAD_CSV, 4, "Field '%s'='%s' handler ret='%s'\n",
							csv_field->name, s,
							"CSV_INVALID_FIELD_BREAK");
					state->badline = 1;
					break;
				} else if (res == CSV_VALID_FIELD_BREAK) {
					/* found a valid field, but caller told us
					 * nothing interesting on this line
					 */
					debug(LOAD_CSV, 5, "Field '%s'='%s' handler ret='%s'\n",
							csv_field->name, s,
							"CSV_VALID_FIELD_BREAK");
					break;
				
				} else if (res == CSV_VALID_FIELD_SKIP) {
					debug(LOAD_CSV, 5, "Field '%s' told us to skip %d fields\n",
							csv_field->name, state->skip);
					for (i = 0; i < state->skip && s != NULL; i++) {
						s = cf->csv_strtok_r(NULL, cf->delim, &save_s, sd, se);
						debug(LOAD_CSV, 6, "Skipping %s\n", s);
					}
					if (s == NULL)
						break;
				} else if (res == CSV_CATASTROPHIC_FAILURE) {
					/* FATAL ERROR like no more memory*/
					debug(LOAD_CSV, 1,  "File %s line %lu : fatal error, aborting\n",
							cf->file_name, state->line);
					debug_timing_end(2);
					return -2;
				}
			} else {/* if csv_>field */
				debug(LOAD_CSV, 5, "No field handler for pos=%d data='%s'\n",
						pos, s);
			}
			s = cf->csv_strtok_r(NULL, cf->delim, &save_s, sd, se);
		} /* while s */
		if (state->mandatory_fields < cf->mandatory_fields) {
			state->badline++;
			debug(LOAD_CSV, 3, "File %s line %lu, not enough fields: %d, requires: %d\n",
					cf->file_name, state->line, state->mandatory_fields, cf->mandatory_fields);
		}

		if (cf->endofline_callback) {
			res = cf->endofline_callback(state, data);
			if (res == CSV_CATASTROPHIC_FAILURE) {/* FATAL ERROR like no more memory*/
				debug(LOAD_CSV, 1,  "File %s line %lu : fatal error, aborting\n",
						cf->file_name, state->line);
				debug_timing_end(2);
				return res;
			} else if (res == CSV_END_FILE) {
				debug(LOAD_CSV, 4, "line %lu : endofline callback asks to stop\n",
						state->line);
				break;
			}
		}
		if (state->badline)
			badlines++;
	} while ((s = st_getline_truncate(f, sizeof(buffer), &i, &res)) != NULL);

	/* end of file */
	if (cf->endoffile_callback)
		res = cf->endoffile_callback(state, data);
	else
		res = CSV_VALID_FILE;
	debug(LOAD_CSV, 2, "File %s Parsed %lu lines, %lu good, %lu bad\n",
			cf->file_name, state->line, state->line - badlines, badlines);
	debug_timing_end(2);
	return res;
}

static void free_csv_field(struct csv_field *cf)
{
	int i;

	if (cf == NULL)
		return;
	for (i = 0; ; i++) {
		if (cf[i].name == NULL)
			return;
		st_free_string(cf[i].name);
		cf[i].name = NULL;
	}
}

int generic_load_csv(const char *filename, struct csv_file *cf,
		struct csv_state *state, void *data)
{
	struct st_file *f;
	char buffer[CSV_MAX_LINE_LEN];
	char *s;
	int res, res2 = 0;

	if (cf->csv_strtok_r == NULL) {
		fprintf(stderr, "coding error:  no strtok function provided\n");
		return -2;
	}
	f = st_open(filename, 128000);
	if (f == NULL) {
		fprintf(stderr, "cannot open %s for reading\n", filename);
		return CSV_CANNOT_OPEN_FILE;
	}
	s = st_getline_truncate(f, sizeof(buffer), &res2, &res);
	if (s == NULL) {
		fprintf(stderr, "empty file %s\n", filename ? filename : "<stdin>");
		st_close(f);
		return CSV_EMPTY_FILE;
	} else if (res) {
		fprintf(stderr, "%s CSV header is longer than the allowed size %d\n",
			 (filename ? filename : "<stdin>"), (int)sizeof(buffer));
		st_close(f);
		return CSV_HEADER_TOOLONG;
	}
	res = read_csv_header(s, cf);
	if (res < 0) {
		st_close(f);
		return res;
	}
	if (cf->validate_header) {
		res2 = cf->validate_header(cf, data);
		if (res2 < 0) {
			st_close(f);
			return res2;
		}
	}
	state->line = 0;
	if (res == CSV_HEADER_FOUND) /* first line was a header, no need to put initial_buff */
		res = read_csv_body(f, cf, state, data, NULL);
	else if (res == CSV_NO_HEADER) /* we need to pass initial buff */
		res = read_csv_body(f, cf, state, data, s);
	else {
		fprintf(stderr, "BUG at %s line %d, invalid res=%d\n", __FILE__, __LINE__, res);
		res = -3;
	}
	st_close(f);
	return res;
}

/* strcmp could be inlined so we need this */
int generic_header_cmp(const char *s1, const char *s2)
{
	return strcmp(s1, s2);
}

int init_csv_file(struct csv_file *cf, const char *file_name, int max_fields,
		const char *delim,
		char string_delim, char string_delim_escape,
		char * (*func)(char *, const char *, char **, char, char))
{
	if (cf == NULL)
		return 0;
	/* mandatory fields */
	cf->csv_field = st_malloc(sizeof(struct csv_field) * max_fields, "CSV Field");
	if (cf->csv_field == NULL) /* unlikely but .. */
		return -1;
	cf->csv_field_sorted = NULL;
	debug(CSVHEADER, 3, "Initializing CSV file %s max fields %d\n", file_name, max_fields);
	debug(CSVHEADER, 3, "delim '%c' delim escape '%c'\n", string_delim, string_delim_escape);
	cf->delim	 = delim;
	cf->csv_strtok_r = func;
	cf->file_name    = (file_name ? file_name : "<stdin>");
	cf->max_fields   = max_fields;
	cf->mandatory_fields = 0;
	cf->string_delim 	= string_delim;
	cf->string_delim_escape = string_delim_escape;
	/* optional fields */
	cf->is_header		 = NULL;
	cf->validate_header	 = NULL;
	cf->default_handler	 = NULL;
	cf->endofline_callback	 = NULL;
	cf->startofline_callback = NULL;
	cf->endoffile_callback	 = NULL;
	cf->header_field_compare = generic_header_cmp;
	cf->num_fields_registered = 0;
	return 1;
}


void free_csv_file(struct csv_file *cf)
{
	if (cf->csv_field != NULL) {
		free_csv_field(cf->csv_field);
		st_free(cf->csv_field, sizeof(struct csv_field) * cf->max_fields);
	}
	if (cf->csv_field_sorted != NULL) {
		/* CSV fields referenced by CSV field sorted are not allocated, they are COPYED
		* from csv_field (which has been freed above */
		st_free(cf->csv_field_sorted, sizeof(struct csv_field) * (cf->num_fields + 1));
		cf->csv_field_sorted = NULL;
	}
}

void init_csv_state(struct csv_state *cs, const char *file_name)
{
	memset(cs, 0, sizeof(struct csv_state));
	cs->file_name = (file_name ? file_name : "<stdin>");
}

int register_csv_field(struct csv_file *csv_file, char *field_name,
		enum csv_mandatory_field mandatory,
		int pos, int default_pos,
		int (*handle)(char *token, void *data, struct csv_state *state))
{
	int i;
	struct csv_field *cf;
	char *name;

	if (csv_file == NULL)
		return 0;
	if (field_name == NULL) {
		fprintf(stderr, "BUG, %s called with NULL field_name\n", __func__);
		return -6;
	}
	i  = csv_file->num_fields_registered;
	cf = csv_file->csv_field;
	if (i == csv_file->max_fields - 1) {
		debug(CSVHEADER, 1, "Cannot register more than %d fields, dropping '%s\n",
				i, field_name);
		return -1;
	}
	name = st_strdup(field_name);
	/* in very unlikely case malloc has failed, we are in deep trouble;
	 * release all resources
	 */
	if (name == NULL) {
		free_csv_file(csv_file);
		return CSV_ENOMEM;
	}
	if (mandatory)
		csv_file->mandatory_fields++;
	cf[i].name        = name;
	cf[i].handle      = handle;
	cf[i].mandatory   = mandatory;
	cf[i].pos         = pos;
	cf[i].default_pos = default_pos;
	cf[i + 1].name    = NULL;
	csv_file->num_fields_registered++;
	debug(CSVHEADER, 3, "Registering handler #%d '%s'\n", i, name);
	return i;
}
