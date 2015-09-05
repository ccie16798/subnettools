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
#include "st_scanf.h"
#include "st_printf.h"
#include "generic_csv.h"
#include "generic_expr.h"
#include "st_routes.h"
#include "ipam.h"

int alloc_ipam_file(struct ipam_file *sf, unsigned long n, int ea_nr) {
	if (n > SIZE_T_MAX / sizeof(struct ipam)) { /* being paranoid */
		fprintf(stderr, "error: too much memory requested for struct ipam\n");
		return -1;
	}
	sf->lines = st_malloc(sizeof(struct ipam) * n, "ipam_file");
	if (sf->lines == NULL) {
		sf->nr = sf->max_nr = 0;
		return -1;
	}
	sf->nr = 0;
	sf->max_nr = n;
	sf->ea_nr = ea_nr;
	sf->ea = st_malloc(ea_nr * sizeof(struct ipam_ea), "ipam_ea");
	if (sf->ea == NULL) {
		sf->nr = sf->max_nr = 0;
		free(sf->lines);
		sf->lines = NULL;
		return -1;
	}
	return 0;
}

int alloc_ea(struct ipam_file *sf, int i) {
	struct ipam_ea *ea;
	int j;

	ea = st_malloc(sf->ea_nr * sizeof(struct ipam_ea), "ipam_ea");
	if (ea == NULL)
		return -1;
	for (j = 0; j < sf->ea_nr; j++)
		ea[j].name = sf->ea[j].name;
	sf->lines[i].ea    = ea;
	sf->lines[i].ea_nr = sf->ea_nr;
	return 0;
}

static void free_ipam_ea(struct ipam *ipam) {
	int i;

	for (i = 0; i < ipam->ea_nr; i++)
		free(ipam->ea[i].value);
	free(ipam->ea);
}

void free_ipam_file(struct ipam_file *sf) {
	int i;

	for (i = 0; i < sf->nr; i++)
		free_ipam_ea(&sf->lines[i]);
	free(sf->ea);
	free(sf->lines);
}

static int ipam_prefix_handle(char *s, void *data, struct csv_state *state) {
	struct ipam_file *sf = data;
	int res;
	struct subnet subnet;

	res = get_subnet_or_ip(s, &subnet);
	if (res < 0) {
		debug(LOAD_CSV, 2, "invalid IP %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	copy_subnet(&sf->lines[sf->nr].subnet,  &subnet);
	return CSV_VALID_FIELD;
}

static int ipam_mask_handle(char *s, void *data, struct csv_state *state) {
	struct ipam_file *sf = data;
	int mask = string2mask(s, 21);

	if (mask < 0) {
		debug(LOAD_CSV, 2, "invalid mask %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->lines[sf->nr].subnet.mask = mask;
	return CSV_VALID_FIELD;
}

static int ipam_ea_handle(char *s, void *data, struct csv_state *state) {
        struct ipam_file *sf = data;
	int ea_nr = state->state[0];
	int found = 0;
	char *z;

	z = strdup(s); /* s cant be NULL here so strdup safe */
	if (z == NULL) {
		fprintf(stderr, "unable to alloc memory, need to abort\n");
		return CSV_CATASTROPHIC_FAILURE; /* FIXME or not ??? */
	}
	for (ea_nr = 0; ea_nr < sf->ea_nr; ea_nr++) {
		if (!strcmp(state->csv_field, sf->ea[ea_nr].name)) {
			found = 1;
			break;
		}

	}
	if (found == 0) {
		debug(IPAM, 2, "No EA match field '%s'\n",  state->csv_field);
		return CSV_INVALID_FIELD_BREAK;
	}
	debug(IPAM, 6, "Found %s = %s\n",  sf->lines[sf->nr].ea[ea_nr].name, z);
	sf->lines[sf->nr].ea[ea_nr].value = z;
	state->state[0]++;
	return CSV_VALID_FIELD;
}

static int ipam_endofline_callback(struct csv_state *state, void *data) {
	struct ipam_file *sf = data;
	struct ipam *new_r;
	int res;

	if (state->badline) {
		debug(LOAD_CSV, 1, "%s : invalid line %lu\n", state->file_name, state->line);
		return -1;
	}
	sf->nr++;
	if  (sf->nr == sf->max_nr) {
		sf->max_nr *= 2;
		if (sf->max_nr > SIZE_T_MAX / sizeof(struct ipam)) {
			fprintf(stderr, "error: too much memory requested for struct ipam\n");
			return CSV_CATASTROPHIC_FAILURE;
		}
		new_r = st_realloc(sf->lines,  sizeof(struct ipam) * sf->max_nr, "ipam");
		if (new_r == NULL)
			return  CSV_CATASTROPHIC_FAILURE;
		sf->lines = new_r;
	}
	memset(&sf->lines[sf->nr], 0, sizeof(struct ipam));
	res = alloc_ea(sf, sf->nr);
	if (res < 0)
		return  CSV_CATASTROPHIC_FAILURE;
	state->state[0] = 0; /* state[0] holds the num of the EA */
	return CSV_CONTINUE;
}

int load_ipam(char  *name, struct ipam_file *sf, struct st_options *nof) {
	struct csv_field *csv_field;
	struct csv_file cf;
	struct csv_state state;
	char *s;
	int i, res, ea_nr = 0;

	ea_nr = count_char(nof->ipam_ea, ',') + 1;
	csv_field = st_malloc((ea_nr + 4) * sizeof(struct csv_field), "IPAM CSV field");
	if (csv_field == NULL)
		return -1;
	init_csv_file(&cf, name, csv_field, nof->ipam_delim, &simple_strtok_r);
	cf.endofline_callback = ipam_endofline_callback;
	init_csv_state(&state, name);

	csv_field[0].name	 =  "address*";
	csv_field[0].pos	 =  0;
	csv_field[0].default_pos =  0;
	csv_field[0].mandatory	 =  1;
	csv_field[0].handle	 =  &ipam_prefix_handle;
	csv_field[1].name	 =  "netmask_dec";
	csv_field[1].pos	 =  0;
	csv_field[1].default_pos =  0;
	csv_field[1].mandatory	 =  1;
	csv_field[1].handle	 =  &ipam_mask_handle;
	csv_field[2].name	 = NULL;
	if (nof->ipam_prefix_field[0])
		csv_field[0].name = nof->ipam_prefix_field;
	if (nof->ipam_mask[0])
		csv_field[1].name = nof->ipam_mask;

	debug(IPAM, 3, "Parsing EA : '%s'\n", nof->ipam_ea);
	ea_nr = 0;
	s = strtok(nof->ipam_ea, ",");
	/* getting Extensible attributes from config file of cmd_line */
	while (s) {
		ea_nr++;
		debug(IPAM, 3, "Registering Extended Attribute : '%s'\n", s);
		register_csv_field(csv_field, s, 0, ipam_ea_handle);
		s = strtok(NULL, ",");
	}
	debug(IPAM, 5, "Collected %d Extended Attributes\n", ea_nr);
	res = alloc_ipam_file(sf, 16192, ea_nr);
	if (res < 0) {
		free(csv_field);
		return -2;
	}
	for (i = 0; i <  ea_nr; i++)
		sf->ea[i].name = csv_field[i + 2].name;
	memset(&sf->lines[0], 0, sizeof(struct ipam));
	res = alloc_ea(sf, 0);
	if (res < 0) {
		free(csv_field);
		return res;
	}
	res = generic_load_csv(name, &cf, &state, sf);
	free(csv_field);
	return res;
}

int fprint_ipamfilter_help(FILE *out) {
	return fprintf(out, "IPAM lines can be filtered on :\n"
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
			"- '~' (st_scanf regular expression)\n");
}

static int ipam_filter(char *s, char *value, char op, void *object) {
	struct ipam *ipam = object;
	struct subnet subnet;
	int res, j, err;
	int found = 0;
	int a, b;

	debug(FILTER, 8, "Filtering '%s' %c '%s'\n", s, op, value);
	if (!strcmp(s, "prefix")) {
		res = get_subnet_or_ip(value, &subnet);
		if (res < 0) {
			debug(FILTER, 1, "Filtering on prefix %c '%s',  but it is not an IP\n", op, value);
			return -1;
		}
		res = subnet_compare(&ipam->subnet, &subnet);
		switch (op) {
		case '=':
			return (res == EQUALS);
			break;
		case '#':
			return !(res == EQUALS);
			break;
		case '<':
			return subnet_is_superior(&ipam->subnet, &subnet);
			break;
		case '>':
			return !subnet_is_superior(&ipam->subnet, &subnet) && res != EQUALS;
			break;
		case '{':
			return (res == INCLUDED || res == EQUALS);
			break;
		case '}':
			return (res == INCLUDES || res == EQUALS);
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for prefix\n", op);
			return -1;
		}
	}
	else if (!strcmp(s, "mask")) {
		res =  string2mask(value, 42);
		if (res < 0) {
			debug(FILTER, 1, "Filtering on mask %c '%s',  but it is valid\n", op, value);
			return -1;
		}
		switch (op) {
		case '=':
			return (ipam->subnet.mask == res);
			break;
		case '#':
			return !(ipam->subnet.mask == res);
			break;
		case '<':
			return (ipam->subnet.mask < res);
			break;
		case '>':
			return (ipam->subnet.mask > res);
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for mask\n", op);
			return -1;
		}
	}
	else {
		for (j = 0; j < ipam->ea_nr; j++) {
			if (!strcmp(s, ipam->ea[j].name)) {
				found = 1;
				break;
			}
		}
		if (found == 0) {
			debug(FILTER, 1, "Cannot filter on attribute '%s'\n", s);
			return 0;
		}
		debug(IPAM, 5, "We will filter on EA: '%s'\n", ipam->ea[j].name);
		if (ipam->ea[j].value == NULL) /* EA Value has not been set */
			return 0;
		switch(op) {
		case '=':
			return (!strcmp(ipam->ea[j].value, value));
			break;
		case '#':
			return (strcmp(ipam->ea[j].value, value));
			break;
		case '~':
			res = st_sscanf(ipam->ea[j].value, value);
			return (res < 0 ? 0 : 1);
			break;
		case '<':
		case '>':
			b = string2int(value, &err);
			if (err < 0) {
				debug(FILTER, 1, "Cannot interpret Field '%s' as an INT\n", value);
				return -1;
			}
			a = string2int(ipam->ea[j].value, &err);
			/* if Extended Attribute is null we don't return an error, just no match
			*/
			if (err < 0) {
				debug(FILTER, 4, "Cannot interpret EA '%s' as an INT\n", ipam->ea[j].value);
				return 0;
			}
			if (op == '>')
				return (a > b);
			else
				return (b > a);
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for Extended Attribute\n", op);
			return -1;
			break;
		}

	}
}

int ipam_file_filter(struct ipam_file *sf, char *expr) {
	int i, j, res, len;
	struct generic_expr e;
	struct ipam *new_ipam;

	if (sf->nr == 0)
		return 0;
	debug_timing_start(2);
	init_generic_expr(&e, expr, ipam_filter);

	new_ipam = st_malloc(sf->max_nr * sizeof(struct ipam), "struct ipam");
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
			free(new_ipam);
			debug_timing_end(2);
			return -1;
		}
		if (res) {
			st_debug(FILTER, 5, "Matching filter '%s' on %P\n", expr, sf->lines[i].subnet);
			memcpy(&new_ipam[j], &sf->lines[i], sizeof(struct ipam));
			j++;
		} else
			free_ipam_ea(&sf->lines[i]);
	}
	free(sf->lines);
	sf->lines = new_ipam;
	sf->nr = j;
	debug_timing_end(2);
	return 0;
}

int populate_sf_from_ipam(struct subnet_file *sf, struct ipam_file *ipam) {
	int i, j, res;
	int found_mask, found_j;

	for (i = 0; i < sf->nr; i++) {
		sf->routes[i].ea_nr = ipam->ea_nr + 1;
		sf->routes[i].ea = st_realloc(sf->routes[i].ea,
				(ipam->ea_nr + 1) * sizeof(struct ipam_ea), "routes EA");
		if (sf->routes[i].ea == NULL)
			return -1;
		found_mask = -1;
		found_j = 0;
		for (j = 0; j < ipam->nr; j++) {
			res = subnet_compare(&sf->routes[i].subnet, &ipam->lines[j].subnet);
			if (res == EQUALS) {
				found_mask = ipam->lines[j].subnet.mask;
				st_debug(IPAM, 4, "found exact match %P\n", ipam->lines[j].subnet);
				found_j = j;
				break; /* we break on exact match */
			} else if (res == INCLUDED) {
				st_debug(IPAM, 4, "found included match %P\n", ipam->lines[j].subnet);
				if (ipam->lines[j].subnet.mask < found_mask)
					continue; /* we have a better ùask */
				found_mask = ipam->lines[j].subnet.mask;
				found_j = j;
			}
		}
		if (found_mask == -1) {
			for (j = 0; j < ipam->ea_nr; j++) {
				sf->routes[i].ea[j + 1].name  = ipam->ea[j].name;
				sf->routes[i].ea[j + 1].value = NULL;
			}
		} else {
			for (j = 0; j < ipam->ea_nr; j++) {
				sf->routes[i].ea[j + 1].name  = ipam->ea[j].name;
				sf->routes[i].ea[j + 1].value = st_strdup(ipam->lines[found_j].ea[j].value);
			}
		}

	}
	return 1;
}
