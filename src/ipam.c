/*
 * Code to read IPAM CSV files
 *
 * Copyright (C) 2014,2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "st_options.h"
#include "ipam.h"
#include "utils.h"
#include "generic_csv.h"

int alloc_ipam_file(struct ipam_file *sf, unsigned long n) {
	if (n > SIZE_T_MAX / sizeof(struct ipam)) { /* being paranoid */
		fprintf(stderr, "error: too much memory requested for struct ipam\n");
		return -1;
	}
	sf->routes = malloc(sizeof(struct ipam) * n);
	if (sf->routes == NULL) {
		fprintf(stderr, "Cannot alloc  memory (%lu Kbytes) for sf->ipam\n",
				n * sizeof(struct ipam));
		sf->nr = sf->max_nr = 0;
		return -1;
	}
	debug(MEMORY, 3, "Allocated %lu bytes for subnet_file\n",  sizeof(struct ipam) * n);
	sf->nr = 0;
	sf->max_nr = n;
	return 0;
}

void free_ipam_file(struct ipam_file *sf) {
	int i, j;

	for (i = 0; i < sf->nr; i++) {
		for (j = 0; j < sf->ea_nr; j++)
			free(sf->routes[i].ea[j].value);
		free(sf->routes[i].ea);
	}
	free(sf->routes);
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
	copy_subnet(&sf->routes[sf->nr].subnet,  &subnet);
	return CSV_VALID_FIELD;
}

static int ipam_mask_handle(char *s, void *data, struct csv_state *state) {
	struct ipam_file *sf = data;
	int mask = string2mask(s, 21);

	if (mask < 0) {
		debug(LOAD_CSV, 2, "invalid mask %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].subnet.mask = mask;
	return CSV_VALID_FIELD;
}

static int ipam_ea_handle(char *s, void *data, struct csv_state *state) {
        struct ipam_file *sf = data;
	int ea_nr = state->state[0];
	char *z;

	if (s == NULL) {
		sf->routes[sf->nr].ea[ea_nr].value = NULL;
		state->state[0]++;
		return CSV_VALID_FIELD;
	}
	z = strdup(s);
	if (z == NULL) {
		debug(LOAD_CSV, 1, "Unable to allocate memory\n");
		return CSV_CATASTROPHIC_FAILURE;
	}
	sf->routes[sf->nr].ea[ea_nr].value = z;
	state->state[0]++;
	return CSV_VALID_FIELD;
}


static int ipam_endofline_callback(struct csv_state *state, void *data) {
	struct ipam_file *sf = data;
	struct ipam *new_r;
	struct ipam_ea *ea;

	if (state->badline) {
		debug(LOAD_CSV, 1, "%s : invalid line %lu\n", state->file_name, state->line);
		return -1;
	}
	sf->nr++;
	if  (sf->nr == sf->max_nr) {
		sf->max_nr *= 2;
		debug(MEMORY, 3, "need to reallocate %lu bytes\n", sf->max_nr * sizeof(struct ipam));
		if (sf->max_nr > SIZE_T_MAX / sizeof(struct ipam)) {
			fprintf(stderr, "error: too much memory requested for struct route\n");
			return CSV_CATASTROPHIC_FAILURE;
		}
		new_r = realloc(sf->routes,  sizeof(struct ipam) * sf->max_nr);
		if (new_r == NULL) {
			fprintf(stderr, "unable to reallocate, need to abort\n");
			return  CSV_CATASTROPHIC_FAILURE;
		}
		sf->routes = new_r;
	}
	memset(&sf->routes[sf->nr], 0, sizeof(struct ipam));
	ea = malloc(sizeof(struct ipam_ea) * sf->ea_nr);
	if (ea == NULL) {
		fprintf(stderr, "unable to allocate memory for Extended Attributes aborting\n");
		return  CSV_CATASTROPHIC_FAILURE;
	}
	sf->routes[sf->nr].ea    = ea;
	sf->routes[sf->nr].ea_nr = sf->ea_nr;
	debug(MEMORY, 3, "Allocated %lu bytes for EA\n",  sizeof(struct ipam_ea) * sf->ea_nr);
	state->state[0] = 0; /* state[0] holds the num of the EA */
	return CSV_CONTINUE;
}

int load_ipam(char  *name, struct ipam_file *sf, struct st_options *nof) {
	struct csv_field csv_field[50] = {
		{ "address*"	, 0,  0, 1, &ipam_prefix_handle },
		{ "netmask_dec"	, 0,  0, 1, &ipam_mask_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;
	char *s;

	if (nof->ipam_prefix_field[0])
		csv_field[0].name = nof->ipam_prefix_field;
	if (nof->ipam_mask[0])
		csv_field[1].name = nof->ipam_mask;

	debug(IPAM, 3, "Parsing EA : '%s'\n", nof->ipam_EA_name);
	s = strtok(nof->ipam_EA_name, ",");
	while (s) {
		debug(IPAM, 3, "Registering Extended Attribute : '%s'\n", s);
		register_csv_field(csv_field, s, 0, ipam_ea_handle);
		s = strtok(NULL, ",");
	}
	init_csv_file(&cf, name, csv_field, nof->ipam_delim, &simple_strtok_r);
	cf.endofline_callback = ipam_endofline_callback;
	init_csv_state(&state, name);

	if (alloc_ipam_file(sf, 16192) < 0)
		return -2;
	memset(&sf->routes[0], 0, sizeof(struct ipam));
	sf->routes[0].ea = malloc(sizeof(struct ipam_ea) * sf->ea_nr);
	if (sf->routes[0].ea == NULL) {
		fprintf(stderr, "unable to allocate memory for Extended Attributes aborting\n");
		return  CSV_CATASTROPHIC_FAILURE;
	}
	return generic_load_csv(name, &cf, &state, sf);
}
