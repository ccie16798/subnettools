/*
 * function to print compile-time limits of subnet-tools
 *
 * Copyright (C) 2018 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#include <stdio.h>
#include "iptools.h"
#include "heap.h"
#include "st_routes.h"
#include "generic_csv.h"
#include "st_printf.h"
#include "st_scanf.h"
#include "generic_expr.h"
#include "st_routes_csv.h"
#include "subnet_tool.h"
#include "bgp_tool.h"
#include "ipam.h"
#include "config_file.h"

#define STRINGIFY(x) #x, x
struct st_limits {
	char *s;
	unsigned long value;
	char *comment;
};

/* very naive, could use a generic algorithm */
static void naive_sprint_large(char *buffer, unsigned long n)
{
	if ((sizeof(unsigned long) > 4) && (n > 1000000000000UL))
		sprintf(buffer, "%lu*10^12", n / 1000000000000UL);
	else if (n > 1000000000UL)
		sprintf(buffer, "%lu*10^9", n / 1000000000UL);
	else if (n > 1000000UL)
		sprintf(buffer, "%lu*10^6", n / 1000000UL);
	else
		sprintf(buffer, "%lu", n);
}

const struct st_limits st_limits[] = {
	{ STRINGIFY(MAX_DELIM),			"Max number of delims used by '-d' option" },
	{ STRINGIFY(FMT_LEN),			"Max size of fmt string, used by option or from config file" },
	{ STRINGIFY(CSV_MAX_FIELD_LENGTH),	"Max length of a CSV header field name" },
	{ STRINGIFY(CSV_MAX_LINE_LEN),		"Max length of a line a CSV; long lines will be truncated" },
	{ STRINGIFY(CSV_MAX_LINE_NUMBER),	"Max number of lines in a CSV" },
	{ STRINGIFY(MAX_COLLECTED_EA),		"Max size of the option string telling which EA to collect" },
	{ STRINGIFY(FSCANF_LINE_LENGTH),	"Size of fscanf internal buffer" },
	{ STRINGIFY(ST_MAX_DEVNAME_LEN),	"Max device name length" },
	{ STRINGIFY(SF_MAX_ROUTES_NUMBER),	"Max number of routes in a in-memory subnet file" },
	{ STRINGIFY(SF_BGP_MAX_ROUTES_NUMBER),	"Max number of routes in a in-memory BGP subnet file" },
	{ STRINGIFY(IPAM_MAX_LINE_NUMBER),	"Max number of routes in a in-memory IPAM file" },
	{ STRINGIFY(HEAP_MAX_NR),		"Max number of objects in a heap structure" },
	{ STRINGIFY(MAX_EA_NUMBER),		"Max number of Extended Attributes attached to a subnet" },
	{ STRINGIFY(ST_OBJECT_MAX_STRING_LEN),	"Max length of string collected with st_scanf(\"%s\") and variants" },
	{ STRINGIFY(ST_VSCANF_MAX_OBJECTS),	"Max number of objects that can be collected with st_scanf functions" },
	{ STRINGIFY(MAX_COLLECTED_OBJECTS),	"Max number of objects that can be collected inside one expression '(EXPR)' of st_scanf" },
	{ STRINGIFY(ST_PRINTF_MAX_STRING_SIZE),	"Max length of a string printed by st_printf(\"%s\");" },
	{ STRINGIFY(ST_VSPRINTF_BUFFER_SIZE),	"Max length of the output buffer of st_printf" },
	{ STRINGIFY(CONFFILE_MAX_LEN),		"Max number of line in a config file" },
	{ STRINGIFY(CONFFILE_MAX_LINE_LEN),	"Max line length in a config file" },
	{ NULL, 0 }
};

void print_stlimits(FILE *f)
{
	int i;
	char feurbu[72];

	for (i = 0; ; i++) {
		if (st_limits[i].s == NULL)
			break;
		naive_sprint_large(feurbu, st_limits[i].value);
		fprintf(f, "%-25s: %-15s, %s\n", st_limits[i].s, feurbu,
			st_limits[i].comment);
	}
}
