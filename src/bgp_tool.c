/*
 * bgp_tool : tools for manipulating BGP tables
 *
 * Copyright (C) 2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
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
#include "bgp_tool.h"

#define SIZE_T_MAX ((size_t)0 - 1)

void fprint_bgp_route(FILE *output, struct bgp_route *route) {
	st_fprintf(output, "%d;%s;%s;%16P;%16I;%10d;%10d;%10d;%s\n",
			route->valid,
			(route->type == 'i' ? " iBGP" : " eBGP"),
			(route->best == 1 ? "Best" : "  No"),
			route->subnet, route->gw, route->MED,
			route->LOCAL_PREF, route->weight,
			route->AS_PATH);
}

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
	int res, i = 0;
	struct subnet subnet;

	while (isspace(s[i]))
		i++;
	res = get_subnet_or_ip(s + i, &subnet);
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
	int res, i = 0;

	while (isspace(s[i]))
		i++;
	res = string2addr(s + i, &addr, 41);
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

	while (isspace(s[i]))
		i++;
	while (isdigit(s[i])) {
		res *= 10;
		res += s[i];
		i++;
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

	while (isspace(s[i]))
		i++;
	while (isdigit(s[i])) {
		res *= 10;
		res += s[i];
		i++;
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

	while (isspace(s[i]))
		i++;
	while (isdigit(s[i])) {
		res *= 10;
		res += s[i];
		i++;
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

static int bgpcsv_best_handle(char *s, void *data, struct csv_state *state) {
	struct bgp_file *sf = data;

	if (!strcasecmp(s, "BEST"))
		sf->routes[sf->nr].best = 1;
	else
		sf->routes[sf->nr].best = 0;
	return CSV_VALID_FIELD;
}

static int bgpcsv_valid_handle(char *s, void *data, struct csv_state *state) {
	struct bgp_file *sf = data;

	if (!strcmp(s, "1"))
		sf->routes[sf->nr].valid = 1;
	else
		sf->routes[sf->nr].valid = 0;
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

static int bgp_field_compare(const char *s1, const char *s2) {
	int i = 0;

	while (isspace(s1[i]))
		i++;
	return strcmp(s1 + i, s2);
}

int load_bgpcsv(char  *name, struct bgp_file *sf, struct st_options *nof) {
	struct csv_field csv_field[] = {
		{ "prefix"	, 0, 0, 1, &bgpcsv_prefix_handle },
		{ "GW"		, 0, 0, 1, &bgpcsv_GW_handle },
		{ "LOCAL_PREF"	, 0, 0, 1, &bgpcsv_localpref_handle },
		{ "MED"		, 0, 0, 1, &bgpcsv_med_handle },
		{ "WEIGHT"	, 0, 0, 1, &bgpcsv_weight_handle },
		{ "AS_PATH"	, 0, 0, 0, &bgpcsv_aspath_handle },
		{ "BEST"	, 0, 0, 1, &bgpcsv_best_handle },
		{ "V"		, 0, 0, 1, &bgpcsv_valid_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;

        cf.is_header = NULL;
	init_csv_file(&cf, csv_field, nof->delim, &strtok_r);
	cf.endofline_callback   = bgpcsv_endofline_callback;
	cf.header_field_compare = bgp_field_compare;

	if (alloc_bgp_file(sf, 16192) < 0)
		return -2;
	return generic_load_csv(name, &cf, &state, sf);
}

int compare_bgp_file(const struct bgp_file *sf1, const struct bgp_file *sf2, struct st_options *o) {
	int i, j;
	int found, changed, changed_j;

	debug(BGPCMP, 6, "file1 : %ld routes, file2 : %ld routes\n", sf1->nr, sf2->nr);
	for (i = 0; i < sf1->nr; i++) {
		st_debug(BGPCMP, 9, "testing %P via %I\n", sf1->routes[i].subnet, 
					sf1->routes[i].gw);
		if (sf1->routes[i].best == 0 || sf1->routes[i].valid != 1) {
			st_debug(BGPCMP, 5, "%P via %I is not a best route, skipping\n", sf1->routes[i].subnet, 
					sf1->routes[i].gw);
			continue;
		}
		found   = 0;
		changed_j = -1;
		for (j = 0; j < sf2->nr; j++) {
			changed = 0;
			if (sf2->routes[j].best == 0 || sf2->routes[j].valid != 1)
				continue;
			if (subnet_compare(&sf1->routes[i].subnet, &sf2->routes[j].subnet) != EQUALS)
				continue;
			found = 1;
			if (is_equal_ip(&sf1->routes[i].gw, &sf2->routes[j].gw) == 0)
				changed++;
			if (sf1->routes[i].MED != sf2->routes[j].MED)
				changed++;
			if (sf1->routes[i].LOCAL_PREF != sf2->routes[j].LOCAL_PREF)
				changed++;
			if (sf1->routes[i].weight != sf2->routes[j].weight)
				changed++;
			if (sf1->routes[i].type != sf2->routes[j].type)
				changed++;
			if (strcmp(sf1->routes[i].AS_PATH, sf2->routes[j].AS_PATH))
				changed++;
			if (changed != 0)
				changed_j = j;
			else {
				changed_j = -1;
				break;
			}
		}
		if (found == 0) {
			st_fprintf(o->output_file, "WITHDRAWN;"); 
			fprint_bgp_route(o->output_file, &sf1->routes[i]);
			continue;
		}
		if (changed_j == -1) {
			fprintf(o->output_file, "UNCHANGED;");
			fprint_bgp_route(o->output_file, &sf1->routes[i]);
			continue;
		}
		fprintf(o->output_file, "CHANGED  ;");
		fprint_bgp_route(o->output_file, &sf2->routes[changed_j]);
		fprintf(o->output_file, "WAS      ;");
		fprint_bgp_route(o->output_file, &sf1->routes[i]);
	}
	return 1;
}
