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

	return CSV_VALID_FIELD;
}


static int ipam_endofline_callback(struct csv_state *state, void *data) {
	struct ipam_file *sf = data;
	struct ipam *new_r;

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
		memset(&sf->routes[sf->nr], 0, sizeof(struct ipam));
	}
	return CSV_CONTINUE;
}

int load_ipam(char  *name, struct ipam_file *sf, struct st_options *nof) {
	struct csv_field csv_field[50] = {
		{ "address*"	, 0,  3, 1, &ipam_prefix_handle },
		{ "netmask"	, 0,  3, 1, &ipam_mask_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;

	if (nof->ipam_prefix_field[0])
		csv_field[0].name = nof->ipam_prefix_field;
	if (nof->ipam_mask[0])
		csv_field[1].name = nof->ipam_mask;
	init_csv_file(&cf, name, csv_field, nof->ipam_delim, &simple_strtok_r);
	cf.endofline_callback = ipam_endofline_callback;
	init_csv_state(&state, name);

	if (alloc_ipam_file(sf, 16192) < 0)
		return -2;
	memset(&sf->routes[0], 0, sizeof(struct ipam));
	return generic_load_csv(name, &cf, &state, sf);
}
