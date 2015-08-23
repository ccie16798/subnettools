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
#include "utils.h"
#include "st_printf.h"
#include "generic_csv.h"
#include "ipam.h"

int alloc_ipam_file(struct ipam_file *sf, unsigned long n, int ea_nr) {
	if (n > SIZE_T_MAX / sizeof(struct ipam)) { /* being paranoid */
		fprintf(stderr, "error: too much memory requested for struct ipam\n");
		return -1;
	}
	sf->routes = malloc(sizeof(struct ipam) * n);
	if (sf->routes == NULL) {
		fprintf(stderr, "Cannot alloc  memory (%lu Kbytes) for sf->ipam\n",
				n * sizeof(struct ipam) / 1024);
		sf->nr = sf->max_nr = 0;
		return -1;
	}
	debug(MEMORY, 3, "Allocated %lu Kbytes for ipam_file\n",
		  sizeof(struct ipam) * n / 1024);
	sf->nr = 0;
	sf->max_nr = n;
	sf->ea_nr = ea_nr;
	sf->ea = malloc(ea_nr * sizeof(struct ipam_ea));
	if (sf->ea == NULL) {
		fprintf(stderr, "Cannot alloc  memory (%lu bytes) for sf->ea\n",
				ea_nr * sizeof(struct ipam_ea));
		sf->nr = sf->max_nr = 0;
		free(sf->routes);
		sf->routes = NULL;
		return -1;
	}
	debug(MEMORY, 3, "Allocated %lu bytes for ipam_ea\n",  sizeof(struct ipam_ea) * ea_nr);
	return 0;
}

int alloc_ea(struct ipam_file *sf, int i) {
	struct ipam_ea *ea;
	int j;

	ea = malloc(sf->ea_nr * sizeof(struct ipam_ea));
	if (ea == NULL) {
		fprintf(stderr, "Cannot alloc  memory (%lu bytes) for sf->ea\n",
				sf->ea_nr * sizeof(struct ipam_ea));
		return -1;
	}
	debug(MEMORY, 3, "Allocated %lu bytes for ipam_ea\n",  sizeof(struct ipam_ea) * sf->ea_nr);
	for (j = 0; j < sf->ea_nr; j++)
		ea[j].name = sf->ea[j].name;
	sf->routes[i].ea    = ea;
	sf->routes[i].ea_nr = sf->ea_nr;
	return 0;
}

void fprint_ipam_file(FILE *out, struct ipam_file *sf) {
	int i, j;

	for (i = 0; i < sf->nr; i++) {
		st_fprintf(out, "%P;", sf->routes[i].subnet);
		for (j = 0; j < sf->ea_nr; j++)
			fprintf(out, "%s=%s;", sf->routes[i].ea[j].name,
					sf->routes[i].ea[j].value);
		fprintf(out, "\n");
	}

}

void free_ipam_file(struct ipam_file *sf) {
	int i, j;

	for (i = 0; i < sf->nr; i++) {
		for (j = 0; j < sf->ea_nr; j++)
			free(sf->routes[i].ea[j].value);
		free(sf->routes[i].ea);
	}
	free(sf->ea);
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

	z = strdup(s); /* s cant be NULL here */
	if (z == NULL) {
		fprintf(stderr, "unable to alloc memory, need to abort\n");
		return CSV_CATASTROPHIC_FAILURE;
	}
	debug(IPAM, 6, "Found %s = %s\n",  sf->routes[sf->nr].ea[ea_nr].name, z);
	sf->routes[sf->nr].ea[ea_nr].value = z;
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
	res = alloc_ea(sf, sf->nr);
	if (res < 0)
		return  CSV_CATASTROPHIC_FAILURE;
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
	int i, res, ea_nr = 0;

	if (nof->ipam_prefix_field[0])
		csv_field[0].name = nof->ipam_prefix_field;
	if (nof->ipam_mask[0])
		csv_field[1].name = nof->ipam_mask;

	init_csv_file(&cf, name, csv_field, nof->ipam_delim, &simple_strtok_r);
	cf.endofline_callback = ipam_endofline_callback;
	init_csv_state(&state, name);

	debug(IPAM, 3, "Parsing EA : '%s'\n", nof->ipam_EA_name);
	s = strtok(nof->ipam_EA_name, ",");
	/* getting Extensible attributes from config file of cmd_line */
	while (s) {
		ea_nr++;
		debug(IPAM, 3, "Registering Extended Attribute : '%s'\n", s);
		register_csv_field(csv_field, s, 0, ipam_ea_handle);
		s = strtok(NULL, ",");
	}
	debug(IPAM, 5, "Collected %d Extended Attributes\n", ea_nr);
	if (alloc_ipam_file(sf, 16192, ea_nr) < 0)
		return -2;
	for (i = 0; i <  ea_nr; i++)
		sf->ea[i].name = csv_field[i + 2].name;
	memset(&sf->routes[0], 0, sizeof(struct ipam));
	res = alloc_ea(sf, 0);
	if (res < 0)
		return res;
	return generic_load_csv(name, &cf, &state, sf);
}
