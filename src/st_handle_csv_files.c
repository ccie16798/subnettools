/*
 * Subnet_tool ! : a tool that will help network engineers :
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
#include <ctype.h>
#include "debug.h"
#include "iptools.h"
#include "utils.h"
#include "generic_csv.h"
#include "st_printf.h"
#include "st_handle_csv_files.h"

#define SIZE_T_MAX ((size_t)0 - 1)
int alloc_subnet_file(struct subnet_file *sf, unsigned long n) {
	if (n > SIZE_T_MAX / sizeof(struct route)) { /* being paranoid */
		fprintf(stderr, "error: too much memory requested for struct route\n");
		return -1;
	}
	sf->routes = malloc(sizeof(struct route) * n);
	debug(MEMORY, 2, "trying to alloc %lu bytes\n",  sizeof(struct route) * n);
	if (sf->routes == NULL) {
		fprintf(stderr, "error: cannot alloc  memory for sf->routes\n");
		return -1;
	}
	sf->nr = 0;
	sf->max_nr = n;
	return 0;
}

static int netcsv_prefix_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;
	int res;
	struct subnet subnet;

	res = get_subnet_or_ip(s, &subnet);
	if (res < 0) {
		debug(LOAD_CSV, 3, "invalid IP %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	copy_subnet(&sf->routes[sf->nr].subnet,  &subnet);
	return CSV_VALID_FIELD;
}

static int netcsv_mask_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;
	u32 mask = string2mask(s, 21);

	if (mask == BAD_MASK) {
		debug(LOAD_CSV, 3, "invalid mask %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].subnet.mask = mask;
	return CSV_VALID_FIELD;
}

static int netcsv_device_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;

	strxcpy(sf->routes[sf->nr].device, s, sizeof(sf->routes[sf->nr].device));
	if (strlen(s) >= sizeof(sf->routes[sf->nr].device))
		debug(LOAD_CSV, 1, "line %lu STRING device '%s'  too long, truncating to '%s'\n", state->line, s, sf->routes[sf->nr].device);
	return CSV_VALID_FIELD;
}

static int netcsv_GW_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;
	struct ip_addr addr;
	int res;

	res = string2addr(s, &addr, 41);
	if (res != IPV4_A && res != IPV6_A) {  /* we accept that there's no gateway but we treat it has a comment instead */
		strxcpy(sf->routes[sf->nr].comment, s, sizeof(sf->routes[sf->nr].comment));
		if (strlen(s) >= sizeof(sf->routes[sf->nr].comment))
			debug(LOAD_CSV, 1, "line %lu STRING comment '%s'  too long, truncating to '%s'\n", state->line, s, sf->routes[sf->nr].comment);
	} else {
		if (res == sf->routes[sf->nr].subnet.ip_ver) {/* does the gw have same IPversion*/
			copy_ipaddr(&sf->routes[sf->nr].gw, &addr);
		} else {
			memset(&sf->routes[sf->nr].gw, 0, sizeof(struct ip_addr));
			debug(LOAD_CSV, 3, "invalid GW %s line %lu\n", s, state->line);
		}
	}
	return CSV_VALID_FIELD;
}

static int netcsv_comment_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;

	strxcpy(sf->routes[sf->nr].comment, s, sizeof(sf->routes[sf->nr].comment));
	if (strlen(s) >= sizeof(sf->routes[sf->nr].comment))
		debug(LOAD_CSV, 1, "line %lu STRING comment '%s'  too long, truncating to '%s'\n", state->line, s, sf->routes[sf->nr].comment);
	return CSV_VALID_FIELD;
}

static int netcsv_is_header(char *s) {
	if (isalpha(s[0]))
		return 1;
	else
		return 0;
}

static int netcsv_endofline_callback(struct csv_state *state, void *data) {
	struct subnet_file *sf = data;
	struct route *new_r;

	if (state->badline) {
		debug(LOAD_CSV, 3, "Invalid line %lu\n", state->line);
		return -1;
	}
	sf->nr++;
	if  (sf->nr == sf->max_nr) {
		sf->max_nr *= 2;
		debug(MEMORY, 2, "need to reallocate %lu bytes\n", sf->max_nr * sizeof(struct route));
		if (sf->max_nr > SIZE_T_MAX / sizeof(struct route)) {
			debug(MEMORY, 1, "cannot allocate %llu bytes for struct route, too big\n", (unsigned long long)sf->max_nr * sizeof(struct route));
			return CSV_CATASTROPHIC_FAILURE;
		}
		new_r = realloc(sf->routes,  sizeof(struct route) * sf->max_nr);
		if (new_r == NULL) {
			fprintf(stderr, "unable to reallocate, need to abort\n");
			return  CSV_CATASTROPHIC_FAILURE;
		}
		sf->routes = new_r;
	}
	memset(&sf->routes[sf->nr], 0, sizeof(struct route));
	return CSV_CONTINUE;
}

static int netcsv_validate_header(struct csv_field *field) {
	int i;
	/* ENHANCE ME
	 * we could check if ( (prefix AND mask) OR (prefix/mask) )
	 */
	for (i = 0; ; i++) {
		if (field[i].name == NULL)
			break;
	}
	return 1;
}

int load_netcsv_file(char *name, struct subnet_file *sf, struct st_options *nof) {
	/* default netcsv fields */
	struct csv_field csv_field[] = {
		{ "prefix"	, 0, 1, 1, &netcsv_prefix_handle },
		{ "mask"	, 0, 2, 0, &netcsv_mask_handle },
		{ "device"	, 0, 0, 0, &netcsv_device_handle },
		{ "GW"		, 0, 0, 0, &netcsv_GW_handle },
		{ "comment"	, 0, 3, 0, &netcsv_comment_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;

	/* netcsv field may have been set by conf file */
	if (nof->netcsv_prefix_field[0])
		csv_field[0].name = nof->netcsv_prefix_field;
	if (nof->netcsv_mask[0])
		csv_field[1].name = nof->netcsv_mask;
	if (nof->netcsv_device[0])
		csv_field[2].name = nof->netcsv_device;
	if (nof->netcsv_gw[0])
		csv_field[3].name = nof->netcsv_gw;
	if (nof->netcsv_comment[0])
		csv_field[4].name = nof->netcsv_comment;

	init_csv_file(&cf, csv_field, nof->delim, &strtok_r);
	cf.is_header = &netcsv_is_header;
	cf.endofline_callback = &netcsv_endofline_callback;
	cf.validate_header = &netcsv_validate_header;

	if (alloc_subnet_file(sf, 4096) < 0)
		return -2;
	memset(&sf->routes[0], 0, sizeof(struct route));
	return generic_load_csv(name, &cf, &state, sf);
}

static int ipam_comment_handle(char *s, void *data, struct csv_state *state) {
        struct  subnet_file *sf = data;

	if (strlen(s) > 2) /* sometimes comment are fucked and a better one is in EA-Name */
		strxcpy(sf->routes[sf->nr].comment, s, sizeof(sf->routes[sf->nr].comment));
	if (strlen(s) >= sizeof(sf->routes[sf->nr].comment))
                debug(LOAD_CSV, 1, "line %lu STRING comment '%s'  too long, truncating to '%s'\n", state->line, s, sf->routes[sf->nr].comment);
	return CSV_VALID_FIELD;
}

int load_PAIP(char  *name, struct subnet_file *sf, struct st_options *nof) {
	/*
	 * default IPAM fields (Infoblox)
  	 * obviously if you have a different IPAM please describe it in the config file
	 */
	struct csv_field csv_field[] = {
		{ "address*"	, 0,  3, 1, &netcsv_prefix_handle },
		{ "netmask_dec"	, 0,  4, 1, &netcsv_mask_handle },
		{ "EA-Name"	, 0, 16, 1, &netcsv_comment_handle },
		{ "comment"	, 0, 17, 1, &ipam_comment_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;

	if (nof->ipam_prefix_field[0])
		csv_field[0].name = nof->ipam_prefix_field;
	if (nof->ipam_mask[0])
		csv_field[1].name = nof->ipam_mask;
	if (nof->ipam_comment1[0]) {
		csv_field[2].name = nof->ipam_comment1;
		/* if comment1 is set, we set also comment2, even if its NULL
		 * if it is NULL, that means the ipam we have doesnt have a secondary comment field
		 */
		csv_field[3].name = nof->ipam_comment2;
	}
	init_csv_file(&cf, csv_field, nof->ipam_delim, &simple_strtok_r);
        cf.is_header = netcsv_is_header;
	cf.endofline_callback = netcsv_endofline_callback;

	if (alloc_subnet_file(sf, 16192) < 0)
		return -2;
	memset(&sf->routes[0], 0, sizeof(struct route));
	return generic_load_csv(name, &cf, &state, sf);
}
