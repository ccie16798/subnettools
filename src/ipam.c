#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "ipam.h"
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

