/*
 * Code to read IPAM CSV files
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
#include "st_strtok.h"
#include "st_scanf.h"
#include "st_printf.h"
#include "generic_csv.h"
#include "generic_expr.h"
#include "st_routes.h"
#include "ipam.h"
#include "string2ip.h"

#define IPAM_STATIC_REGISTERED_FIELDS 2

int alloc_ipam_file(struct ipam_file *sf, unsigned long n, int ea_nr)
{
	if (n > IPAM_MAX_LINE_NUMBER) {
		fprintf(stderr, "error: too much memory requested for struct ipam\n");
		return -1;
	}
	sf->lines = st_malloc(sizeof(struct ipam_line) * n, "ipam_file");
	if (sf->lines == NULL) {
		sf->nr = sf->max_nr = 0;
		return -1;
	}
	sf->nr     = 0;
	sf->max_nr = n;
	sf->ea_nr  = ea_nr;
	sf->ea     = st_malloc(ea_nr * sizeof(char *), "ipam_ea");
	if (sf->ea == NULL) {
		sf->max_nr = 0;
		sf->ea_nr  = 0;
		st_free(sf->lines, n * sizeof(struct ipam_line));
		sf->lines = NULL;
		return -1;
	}
	return 0;
}

static int alloc_ipam_ea(struct ipam_file *sf, unsigned long i)
{
	struct st_ea *ea;
	int j;

	ea = alloc_ea_array(sf->ea_nr);
	if (ea == NULL) {
		sf->lines[i].ea    = NULL;
		sf->lines[i].ea_nr = 0;
		return -1;
	}
	for (j = 0; j < sf->ea_nr; j++)
		ea[j].name  = sf->ea[j];
	sf->lines[i].ea    = ea;
	sf->lines[i].ea_nr = sf->ea_nr;
	return 0;
}

static void free_ipam_ea(struct ipam_line *ipam)
{
	free_ea_array(ipam->ea, ipam->ea_nr);
	ipam->ea    = NULL;
	ipam->ea_nr = 0;
}

void free_ipam_file(struct ipam_file *sf)
{
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		free_ipam_ea(&sf->lines[i]);
	for (i = 0; i < sf->ea_nr; i++)
		st_free_string(sf->ea[i]);
	st_free(sf->ea, sizeof(char *) * sf->ea_nr);
	st_free(sf->lines, sizeof(struct ipam_line) * sf->max_nr);
	sf->nr     = 0;
	sf->max_nr = 0;
	sf->ea_nr  = 0;
	sf->lines  = NULL;
	sf->ea     = NULL;
}

static int ipam_prefix_handle(char *s, void *data, struct csv_state *state)
{
	struct ipam_file *sf = data;
	int res;

	res = get_subnet_or_ip(s, &sf->lines[sf->nr].subnet);
	if (res < 0) {
		debug(IPAM, 3, "invalid IP %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	return CSV_VALID_FIELD;
}

static int ipam_mask_handle(char *s, void *data, struct csv_state *state)
{
	struct ipam_file *sf = data;
	int mask = string2mask(s, 21);

	if (mask < 0) {
		debug(IPAM, 3, "invalid mask %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->lines[sf->nr].subnet.mask = mask;
	return CSV_VALID_FIELD;
}

static int ipam_ea_handle(char *s, void *data, struct csv_state *state)
{
	struct ipam_file *sf = data;
	int ea_nr;

	/* to get the EA NR, this will depend on the CSV FIELD ID that handle the request
	* we  have csv_id 0 & 1 that are set for prefix and MASK, csv_id 2.... will handle EA
	*/
	ea_nr = state->csv_id - IPAM_STATIC_REGISTERED_FIELDS;
	debug(IPAM, 6, "Found ea#%d %s = %s\n",  ea_nr, sf->ea[ea_nr], s);
	/* we dont care if memory failed on strdup; we continue */
	ea_strdup(&sf->lines[sf->nr].ea[ea_nr], s);
	return CSV_VALID_FIELD;
}

static int ipam_endofline_callback(struct csv_state *state, void *data)
{
	struct ipam_file *sf = data;
	struct ipam_line *new_r;

	if (state->badline) {
		debug(IPAM, 2, "%s : invalid line %lu (use -D ipam:3) to get error for this line\n",
				state->file_name, state->line);
		/* if too much badlines, we will loose time alloc in startofline
		 * and freeing in endofline but whatever
		 */
		free_ipam_ea(&sf->lines[sf->nr]);
		return -1;
	}
	sf->nr++;
	if  (sf->nr == sf->max_nr) {
		if (sf->max_nr * 2 > IPAM_MAX_LINE_NUMBER) {
			fprintf(stderr, "error: too much memory requested for struct ipam_line\n");
			return CSV_CATASTROPHIC_FAILURE;
		}
		new_r = st_realloc(sf->lines,  sizeof(struct ipam_line) * sf->max_nr * 2,
					sizeof(struct ipam_line) * sf->max_nr, "ipam line");
		if (new_r == NULL)
			return  CSV_CATASTROPHIC_FAILURE;
		sf->max_nr *= 2;
		sf->lines = new_r;
	}
	memset(&sf->lines[sf->nr], 0, sizeof(struct ipam_line));
	return CSV_CONTINUE;
}

static int ipam_startofline_callback(struct csv_state *state, void *data)
{
	struct ipam_file *sf = data;
	int res;

	/* alloc memory for the new line */
	res = alloc_ipam_ea(sf, sf->nr);
	if (res < 0)
		return  CSV_CATASTROPHIC_FAILURE;
	return CSV_VALID_FILE;
}

static int ipam_endoffile_callback(struct csv_state *state, void *data)
{
	struct ipam_file *sf = data;

	if (sf->nr == 0) {
		fprintf(stderr, "IPAM file %s has %lu lines, none is valid\n",
				state->file_name, state->line);
		return CSV_INVALID_FILE;
	}
	return CSV_VALID_FILE;
}

int load_ipam(char  *name, struct ipam_file *sf, struct st_options *nof)
{
	struct csv_file cf;
	struct csv_state state;
	char *s;
	int i, res, ea_nr = 0;
	char c;

	c = nof->ipam_comment_delim;
	if (c != '\0' && c != '\'' && c != '"' && c != '`') {
		fprintf(stderr, "invalid ipam comment delim '%c'\n", c);
		return -1;
	}
	c = nof->ipam_comment_delim_escape;
	if (c != '\0' && c != '\\') {
		fprintf(stderr, "invalid ipam comment delim escape char '%c'\n",
				c);
		return -1;
	}
	/*
	 * this ea_nr is just a naive estimate; the ea_nr field could be
	 * badly formatted, like ',EA-Site' or 'comment,'
	 */
	ea_nr = count_char(nof->ipam_ea, ',') + 1;
	if (nof->ipam_delim[1] == '\0')
		res = init_csv_file(&cf, name, ea_nr + 4, nof->ipam_delim,
				nof->ipam_comment_delim,
				nof->ipam_comment_delim_escape,
				&st_strtok_string_r1);
	else
		res = init_csv_file(&cf, name, ea_nr + 4, nof->ipam_delim,
				nof->ipam_comment_delim,
				nof->ipam_comment_delim_escape,
				&st_strtok_string_r);
	if (res < 0)
		return res;
	init_csv_state(&state, name);
	cf.endofline_callback   = ipam_endofline_callback;
	cf.startofline_callback = ipam_startofline_callback;
	cf.endoffile_callback   = ipam_endoffile_callback;

	/* register network and mask handler */
	s = (nof->ipam_prefix_field[0] ? nof->ipam_prefix_field : "address*");
	register_csv_field(&cf, s, mandatory, 0, 0, ipam_prefix_handle);
	s = (nof->ipam_mask[0] ? nof->ipam_mask : "netmask_dec");
	register_csv_field(&cf, s, mandatory, 0, 0, ipam_mask_handle);

	debug(IPAM, 4, "Parsing EA : '%s'\n", nof->ipam_ea);
	i = 0;
	s = strtok(nof->ipam_ea, ",");
	/* getting Extensible attributes from config file of cmd_line */
	while (s) {
		i++;
		debug(IPAM, 4, "Registering Extended Attribute : '%s'\n", s);
		register_csv_field(&cf, s, optional, 0, 0, ipam_ea_handle);
		s = strtok(NULL, ",");
	}
	if (i == 0) {
		fprintf(stderr, "Please specify at least one Extended Attribute\n");
		free_csv_file(&cf);
		return -1;
	}
	ea_nr = i; /* accurate value here */
	if (cf.csv_field == NULL) { /* failed a malloc of csv_field name */
		free_csv_file(&cf);
		return -1;
	}
	debug(IPAM, 5, "Registered %d Extended Attributes\n", i);
	res = alloc_ipam_file(sf, 16192, i);
	if (res < 0) {
		free_csv_file(&cf);
		return res;
	}
	for (i = 0; i < ea_nr; i++) {
		sf->ea[i] = st_strdup(cf.csv_field[i + 2].name);
		if (sf->ea[i] == NULL) {
			debug(IPAM, 5, " Extended Attributes %i is NULL\n", i);
			free_ipam_file(sf);
			free_csv_file(&cf);
			return -1;
		}
	}
	memset(&sf->lines[0], 0, sizeof(struct ipam_line));
	res = generic_load_csv(name, &cf, &state, sf);
	if (res < 0) {
		free_ipam_ea(&sf->lines[0]);
		free_csv_file(&cf);
		free_ipam_file(sf);
		return res;
	}
	free_csv_file(&cf);
	return res;
}

int fprint_ipamfilter_help(FILE *out)
{
	return fprintf(out, "IPAM lines can be filtered on:\n"
			" -prefix\n"
			" -mask\n"
			" -Extended Attributes\n"
			"operator are :\n"
			"- '=' (EQUALS)\n"
			"- '#' (DIFFERENT)\n"
			"- '<' (numerically inferior, if EA is of type INT)\n"
			"- '>' (numerically superior, if EA is of type INT)\n"
			"- '{' (is included (for prefixes))\n"
			"- '}' (includes (for prefixes))\n"
			"- '~' (st_scanf regular expression)\n"
			"- '%%' (st_scanf case insensitive regular expression)\n");
}

static int ipam_filter(const char *s, const char *value, char op, void *object)
{
	struct ipam_line *ipam = object;
	struct subnet subnet;
	int res;

	debug(FILTER, 8, "Filtering '%s' %c '%s'\n", s, op, value);
	if (!strcmp(s, "prefix")) {
		res = get_subnet_or_ip(value, &subnet);
		if (res < 0) {
			debug(FILTER, 1, "Filtering on prefix %c '%s', but it is not an IP\n",
					op, value);
			return -1;
		}
		return subnet_filter(&ipam->subnet, &subnet, op);
	} else if (!strcmp(s, "mask")) {
		res =  string2mask(value, 42);
		if (res < 0) {
			debug(FILTER, 1,
					"Filtering on mask %c '%s', but it is not valid\n",
					op, value);
			return -1;
		}
		switch (op) {
		case '=':
			return (ipam->subnet.mask == res);
		case '#':
			return !(ipam->subnet.mask == res);
		case '<':
			return (ipam->subnet.mask < res);
		case '>':
			return (ipam->subnet.mask > res);
		default:
			debug(FILTER, 1, "Unsupported op '%c' for mask\n", op);
			return -1;
		}
	} else
		return filter_ea(ipam->ea, ipam->ea_nr, s, value, op);
}

int ipam_file_filter(struct ipam_file *sf, char *expr)
{
	unsigned long i, j;
	int res, len;
	struct generic_expr e;
	struct ipam_line *new_ipam;

	if (sf->nr == 0)
		return 0;
	debug_timing_start(2);
	init_generic_expr(&e, expr, ipam_filter);

	new_ipam = st_malloc(sf->nr * sizeof(struct ipam_line), "struct ipam_line");
	if (new_ipam == NULL) {
		debug_timing_end(2);
		return -1;
	}
	j = 0;
	len = strlen(expr);

	for (i = 0; i < sf->nr; i++) {
		e.object = &sf->lines[i];
		res = run_generic_expr(expr, len, &e);
		if (res < 0) {
			fprintf(stderr, "Invalid filter '%s'\n", expr);
			st_free(new_ipam, sf->nr * sizeof(struct ipam_line));
			debug_timing_end(2);
			return -1;
		}
		if (res) {
			st_debug(FILTER, 5, "Matching filter '%s' on %P\n",
					expr, sf->lines[i].subnet);
			memcpy(&new_ipam[j], &sf->lines[i], sizeof(struct ipam_line));
			j++;
		} else
			free_ipam_ea(&sf->lines[i]);
	}
	st_free(sf->lines, sf->max_nr * sizeof(struct ipam_line));
	sf->lines  = new_ipam;
	sf->max_nr = sf->nr;
	sf->nr     = j;
	debug_timing_end(2);
	return 0;
}

int populate_sf_from_ipam(struct subnet_file *sf, struct ipam_file *ipam)
{
	unsigned long i, j, found_j;
	int k, res, sf_ea_nr;
	int found_mask, mask;
	int has_comment = 0, comment_index = -1;
	char **new_ea;

	/*
	 * subnet file sf may already have Extended Attributes
	 * we must append EA from IPAM as well
	 * comment from subnet_file is always overwritten
	 */
	for (j = 0; j < ipam->ea_nr; j++)
		if (!strcasecmp(ipam->ea[j], "comment")) {
			has_comment = 1;
			comment_index = j;
		}
	new_ea = st_realloc(sf->ea, (sf->ea_nr + ipam->ea_nr - has_comment) * sizeof(char *),
			sf->ea_nr * sizeof(char *), "ST route EA array");
	if (new_ea == NULL)
		return -1;
	sf->ea = new_ea;
	debug(IPAM, 4, "File had already %d EA, need to add %d IPAM ea\n",
			sf->ea_nr, ipam->ea_nr);

	sf_ea_nr = sf->ea_nr; /* save original number of Extended Attributes */
	k = sf_ea_nr;
	sf->ea_nr += (ipam->ea_nr - has_comment);
	for (j = 0; j < ipam->ea_nr; j++) {
		if (j != comment_index) {
			sf->ea[k] = st_strdup(ipam->ea[j]);
			k++;
		}
	}

	for (i = 0; i < sf->nr; i++) {
		/* allocating new EA and setting value to NULL */
		res = realloc_route_ea(&sf->routes[i],
				sf->routes[i].ea_nr + ipam->ea_nr - has_comment);

		if (res < 0)
			return res;
		found_mask = -1;
		found_j    = 0;
		for (j = 0; j < ipam->nr; j++) {
			res = subnet_compare(&sf->routes[i].subnet, &ipam->lines[j].subnet);
			if (res == EQUALS) {
				found_mask = ipam->lines[j].subnet.mask;
				st_debug(IPAM, 5, "found exact match %P\n", ipam->lines[j].subnet);
				found_j = j;
				break; /* we break on exact match */
			} else if (res == INCLUDED) {
				st_debug(IPAM, 5, "found included match %P\n",
						ipam->lines[j].subnet);
				mask = ipam->lines[j].subnet.mask;
				if (mask < found_mask)
					continue; /* we have a better mask */
				found_mask = mask;
				found_j    = j;
			}
		}
		k = sf_ea_nr;
		if (found_mask == -1) {
			for (j = 0; j < ipam->ea_nr; j++) {
				/* 'comment' get special treatment; it is included in struct route
				 * by default as route->ea[0]; so if we get 'comment' EA from ipam
				 *  we need to overwrite it with IPAM value and free previous value
				 */
				if (j == comment_index) {
					sf->routes[i].ea[0].name  = ipam->ea[j];
					free_ea(&sf->routes[i].ea[0]);
				} else {
					sf->routes[i].ea[k].name  = ipam->ea[j];
					free_ea(&sf->routes[i].ea[0]);
					k++;
				}
			}
		} else {
			for (j = 0; j < ipam->ea_nr; j++) {
				if (j == comment_index) {
					sf->routes[i].ea[0].name  = ipam->ea[j];
					free_ea(&sf->routes[i].ea[0]);
					ea_strdup(&sf->routes[i].ea[0],
							ipam->lines[found_j].ea[j].value);
				} else {
					sf->routes[i].ea[k].name  = ipam->ea[j];
					free_ea(&sf->routes[i].ea[k]);
					ea_strdup(&sf->routes[i].ea[k],
							ipam->lines[found_j].ea[j].value);
					k++;
				}
			}
		}
	}
	return 1;
}
