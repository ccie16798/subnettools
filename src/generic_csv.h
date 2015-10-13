/*
 * Generic CSV parser functions
 * 
 * Copyright (C) 2014 Etienne Basset <etienne.basset@ensta.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#ifndef GENERIC_CSV
#define GENERIC_CSV

#define CSV_MAX_LINE_LEN 1024

/* csv_field->handler return values */
#define CSV_INVALID_FIELD_BREAK  -1
#define CSV_VALID_FIELD 	  1
#define CSV_VALID_FIELD_BREAK     2
#define CSV_VALID_FIELD_SKIP      3

/* csv_file->endofline return values */
#define CSV_CATASTROPHIC_FAILURE -3 /* unrecoverable failure, like a malloc */
#define CSV_CONTINUE  1
#define CSV_END_FILE  25

/* csv_file->endoffile return values */
#define CSV_VALID_FILE     1
#define CSV_INVALID_FILE  -29

/* generic load csv return values */
#define CSV_CANNOT_OPEN_FILE  -1
#define CSV_EMPTY_FILE        -2
#define CSV_FILE_MAX_SIZE      4  /* state->line reached ULONG_MAX */

/* csv parse header return value ; private */
#define  CSV_HEADER_FOUND   1
#define  CSV_NO_HEADER      2
#define  CSV_BAD_HEADER     -19
#define  CSV_HEADER_TOOLONG -18

/* csv_state is used to store data across calls to csv_field->handle
 * obvious exemple is the line number, but each caller of generic_load_csv
 * SHOULD set any value it requires
 * state can be used to make a decision on FIELD N based on FIELD N-1 etc...
 *
 * As such, it allows to parse 'imperfects' CSV, like IP route tables,
 * where the line formating depends on the type of route etc...
 *
 * a regular CSV file does not need to  use struct csv_state (except line/badline)
 */
struct csv_state {
	unsigned long line; /* current line */
	char *file_name;
	char *csv_field; /* name of the field we crossed */
	int  badline;
	int skip; /* tell the engine to skip fields without increasing the pos counter */
	int state[8]; /* generic states table, csv_field->handle can use it as its wants */
};

struct csv_field {
	char *name;
	int pos; /* pos starts at 1, pos == 0 means it is not set */
	int default_pos; /* used in  CSV files where there is no HEADER, and only if there is no header */
	int mandatory;
	int (*handle)(char *token, void *data, struct csv_state *state);
};

struct csv_file {
	char *file_name;
	struct csv_field *csv_field;
	char *delim; /** delimiteur */
	int max_mandatory_pos ; /* used to track the pos of the last mandatory field */
	/* given a line, try to guess if its a header or plain data; if NULL, the file REQUIRES a header */
	int (*is_header)(char *);
	/* once a header has been parsed, post-validate the required fields are there */
	int (*validate_header)(struct csv_field *);
	/* use to compare CSV header FIELDS name, strcmp by default */
	int (*header_field_compare)(const char *, const char *);
	/* handler for fields where no specific handler is found */
	int (*default_handler)(char *token, void *data, struct csv_state *state);
	int (*endofline_callback)(struct csv_state *state, void *data);
	int (*endoffile_callback)(struct csv_state *state, void *data);
	char * (*csv_strtok_r)(char *s, const char *delim, char **save_ptr);
};

/* will only set the mandatory things (field, delim, and strtok_r function
 * strtok_r possible values are :
 * - simple_strtok_r : doesnt treat consecutive delims as one
 * - strtok_r        : treat consecutives delims as one
 * */
void init_csv_file(struct csv_file *cf, char *file_name, struct csv_field *csv_field, char *delim,
		char * (*strtok_r)(char *s, const char *delim, char **save_ptr));
void init_csv_state(struct csv_state *cs, char *file_name);
int register_csv_field(struct csv_field *cf, char *name, int mandatory,
	int (*handle)(char *token, void *data, struct csv_state *state));
/* this func will open the 'filename' FILE, parse it according to 'cf' and 'state'
 * and usually you'll want to feed a  pointer to a struct whatever in *data
 *
 * @filename : name of the file, stdin is used if filename == NULL
 * @cf       : struct csv_file, must have been init before
 * @state    : a generic state object used by run_body, to store data between callbacks
 * @data     : this will point to a struct you want to fill with data read from the file
 * */
int generic_load_csv(char *filename, struct csv_file *cf, struct csv_state *state, void *data);

#else
#endif
