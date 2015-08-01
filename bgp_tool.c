/*
 * bgp_tool : tools for manipulating BGP tables
 *
 * Copyright (C) 2014,2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "debug.h"
#include "iptools.h"
#include "utils.h"
#include "generic_csv.h"
#include "heap.h"
#include "subnet_tool.h"
#include "st_printf.h"

#define SIZE_T_MAX ((size_t)0 - 1)
int alloc_bgp_file(struct bgp_file *sf, unsigned long n) {
	if (n > SIZE_T_MAX / sizeof(struct bgp_route)) { /* being paranoid */
		fprintf(stderr, "error: too much memory requested for struct route\n");
		return -1;
	}
	sf->routes = malloc(sizeof(struct bgp_route) * n);
	debug(MEMORY, 2, "trying to alloc %lu bytes\n",  sizeof(struct bgp_route) * n);
	if (sf->routes == NULL) {
		fprintf(stderr, "error: cannot alloc  memory for sf->routes\n");
		return -1;
	}
	sf->nr = 0;
	sf->max_nr = n;
	return 0;
}

static int bgpcsv_prefix_handle(char *s, void *data, struct csv_state *state) {
	struct bgp_file *sf = data;
	int res;
	struct subnet subnet;

	res = get_subnet_or_ip(s, &subnet);
	if (res > 1000) {
		debug(LOAD_CSV, 3, "invalid IP %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	copy_subnet(&sf->routes[sf->nr].subnet,  &subnet);

	return CSV_VALID_FIELD;
}

static int bgpcsv_GW_handle(char *s, void *data, struct csv_state *state) {
	struct bgp_file *sf = data;
	struct ip_addr addr;
	int res;

	res = string2addr(s, &addr, 41);
	if (res == sf->routes[sf->nr].subnet.ip_ver) {/* does the gw have same IPversion*/
		copy_ipaddr(&sf->routes[sf->nr].gw, &addr);
	} else {
		memset(&sf->routes[sf->nr].gw, 0, sizeof(struct ip_addr));
		debug(LOAD_CSV, 3, "invalid GW %s line %lu\n", s, state->line);
	}
	return CSV_VALID_FIELD;
}

static int bgpcsv_med_handle(char *s, void *data, struct csv_state *state) {
	struct bgp_file *sf = data;
	int res = 0;
	int i = 0;

	while (isspace(s[i]));
	while (isdigit(s[i])) {
		res *= 10;
		res += s[i];
	}
	if (s[i] != '\0') {
                debug(LOAD_CSV, 1, "line %lu '%s' is not an INT\n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].MED = res;
	return CSV_VALID_FIELD;
}

static int bgpcsv_localpref_handle(char *s, void *data, struct csv_state *state) {
	struct bgp_file *sf = data;
	int res = 0;
	int i = 0;

	while (isspace(s[i]));
	while (isdigit(s[i])) {
		res *= 10;
		res += s[i];
	}
	if (s[i] != '\0') {
                debug(LOAD_CSV, 1, "line %lu '%s' is not an INT\n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].LOCAL_PREF = res;
	return CSV_VALID_FIELD;
}

static int bgpcsv_weight_handle(char *s, void *data, struct csv_state *state) {
	struct bgp_file *sf = data;
	int res = 0;
	int i = 0;

	while (isspace(s[i]));
	while (isdigit(s[i])) {
		res *= 10;
		res += s[i];
	}
	if (s[i] != '\0') {
                debug(LOAD_CSV, 1, "line %lu '%s' is not an INT\n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].weight = res;
	return CSV_VALID_FIELD;
}

static int bgpcsv_aspath_handle(char *s, void *data, struct csv_state *state) {
	struct bgp_file *sf = data;

	strxcpy(sf->routes[sf->nr].AS_PATH, s, sizeof(sf->routes[sf->nr].AS_PATH));
	return CSV_VALID_FIELD;
}

static int bgpcsv_endofline_callback(struct csv_state *state, void *data) {
	struct bgp_file *sf = data;
	struct bgp_route *new_r;

	if (state->badline) {
		debug(LOAD_CSV, 3, "Invalid line %lu\n", state->line);
		return -1;
	}
	sf->nr++;
	if  (sf->nr == sf->max_nr) {
		sf->max_nr *= 2;
		debug(MEMORY, 2, "need to reallocate %lu bytes\n", sf->max_nr * sizeof(struct bgp_route));
		if (sf->max_nr > SIZE_T_MAX / sizeof(struct route)) {
			debug(MEMORY, 1, "cannot allocate %llu bytes for struct route, too big\n", (unsigned long long)sf->max_nr * sizeof(struct bgp_route));
			return CSV_CATASTROPHIC_FAILURE;
		}
		new_r = realloc(sf->routes,  sizeof(struct bgp_route) * sf->max_nr);
		if (new_r == NULL) {
			fprintf(stderr, "unable to reallocate, need to abort\n");
			return  CSV_CATASTROPHIC_FAILURE;
		}
		sf->routes = new_r;
	}
	memset(&sf->routes[sf->nr], 0, sizeof(struct bgp_route));
	return CSV_CONTINUE;
}

int load_bgpcsv(char  *name, struct bgp_file *sf, struct st_options *nof) {
	struct csv_field csv_field[] = {
		{ "prefix"	, 0, 0, 1, &bgpcsv_prefix_handle },
		{ "GW"		, 0, 0, 1, &bgpcsv_GW_handle },
		{ "LOCAL_PREF"	, 0, 0, 1, &bgpcsv_localpref_handle },
		{ "MED"		, 0, 0, 1, &bgpcsv_med_handle },
		{ "WEIGHT"	, 0, 0, 1, &bgpcsv_weight_handle },
		{ "AS_PATH"	, 0, 0, 1, &bgpcsv_aspath_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;

        cf.is_header = NULL;
	cf.endofline_callback = bgpcsv_endofline_callback;
	init_csv_file(&cf, csv_field, nof->delim, &strtok_r);

	if (alloc_bgp_file(sf, 16192) < 0)
		return -2;
	return generic_load_csv(name, &cf, &state, sf);
}


