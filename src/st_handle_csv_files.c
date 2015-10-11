/*
 * routines to load CSV files into memory
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
#include "st_memory.h"
#include "iptools.h"
#include "st_routes.h"
#include "utils.h"
#include "generic_csv.h"
#include "st_printf.h"
#include "bgp_tool.h"
#include "st_handle_csv_files.h"

#define SIZE_T_MAX ((size_t)0 - 1)
int alloc_subnet_file(struct subnet_file *sf, unsigned long n)
{
	if (n > SIZE_T_MAX / sizeof(struct route)) { /* being paranoid */
		fprintf(stderr, "error: too much memory requested for struct route\n");
		return -1;
	}
	sf->routes = st_malloc(sizeof(struct route) * n, "subnet_file");
	if (sf->routes == NULL) {
		sf->nr = sf->max_nr = 0;
		return -1;
	}
	sf->nr     = 0;
	sf->max_nr = n;
	return 0;
}

void free_subnet_file(struct subnet_file *sf)
{
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		free_route(&sf->routes[i]);
	free(sf->routes);
	total_memory -= sf->max_nr * sizeof(struct route);
	sf->routes = NULL;
	sf->nr = sf->max_nr = 0;
}

static int netcsv_prefix_handle(char *s, void *data, struct csv_state *state)
{
	struct subnet_file *sf = data;
	int res;
	struct subnet subnet;
	int mask;

	if (state->state[0])
		mask = sf->routes[sf->nr].subnet.mask;
	res = get_subnet_or_ip(s, &subnet);
	if (res < 0) {
		debug(LOAD_CSV, 2, "invalid IP %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	copy_subnet(&sf->routes[sf->nr].subnet,  &subnet);
	if (state->state[0]) /* if we found a mask before finding a subnet */
		sf->routes[sf->nr].subnet.mask = mask;
	return CSV_VALID_FIELD;
}

static int netcsv_mask_handle(char *s, void *data, struct csv_state *state)
{
	struct subnet_file *sf = data;
	int mask = string2mask(s, 21);

	if (mask < 0) {
		debug(LOAD_CSV, 2, "invalid mask %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].subnet.mask = mask;
	state->state[0] = 1;
	return CSV_VALID_FIELD;
}

static int netcsv_device_handle(char *s, void *data, struct csv_state *state)
{
	struct subnet_file *sf = data;
	int res;

	res = strxcpy(sf->routes[sf->nr].device, s, sizeof(sf->routes[sf->nr].device));
	if (res >= sizeof(sf->routes[sf->nr].device))
		debug(LOAD_CSV, 2, "line %lu STRING device '%s'  too long, truncating to '%s'\n", state->line, s, sf->routes[sf->nr].device);
	return CSV_VALID_FIELD;
}

static int netcsv_GW_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;
	struct ip_addr addr;
	int res;

	res = string2addr(s, &addr, 41);
	if (res != IPV4_A && res != IPV6_A) {  /* we accept that there's no gateway but we treat it has a comment instead */
		/* we dont care if memory alloc failed here */
		ea_strdup(&sf->routes[sf->nr].ea[0], s);
	} else {
		if (res == sf->routes[sf->nr].subnet.ip_ver) {/* does the gw have same IPversion*/
			copy_ipaddr(&sf->routes[sf->nr].gw, &addr);
		} else {
			zero_ipaddr(&sf->routes[sf->nr].gw);
			debug(LOAD_CSV, 2, "invalid GW %s line %lu\n", s, state->line);
		}
	}
	return CSV_VALID_FIELD;
}

static int netcsv_comment_handle(char *s, void *data, struct csv_state *state)
{
	struct subnet_file *sf = data;

	ea_strdup(&sf->routes[sf->nr].ea[0], s);
	return CSV_VALID_FIELD;
}

static int netcsv_is_header(char *s)
{
	if (isalpha(s[0]))
		return 1;
	else
		return 0;
}

static int netcsv_endofline_callback(struct csv_state *state, void *data)
{
	struct subnet_file *sf = data;
	struct route *new_r;
	int res;

	if (state->badline) {
		debug(LOAD_CSV, 1, "%s : invalid line %lu\n", state->file_name, state->line);
		return -1;
	}
	sf->nr++;
	if  (sf->nr == sf->max_nr) {
		if (sf->max_nr * 2 > SIZE_T_MAX / sizeof(struct route)) {
			fprintf(stderr, "error: too much memory requested for struct route\n");
			return CSV_CATASTROPHIC_FAILURE;
		}
		new_r = st_realloc(sf->routes, sizeof(struct route) * 2 * sf->max_nr,
				sizeof(struct route) * sf->max_nr,
				"struct route");
		if (new_r == NULL)
			return  CSV_CATASTROPHIC_FAILURE;
		sf->max_nr *= 2;
		sf->routes = new_r;
	}
	zero_route(&sf->routes[sf->nr]);
	res = alloc_route_ea(&sf->routes[sf->nr], 1);
	if (res < 0)
		return CSV_CATASTROPHIC_FAILURE;
	sf->routes[sf->nr].ea[0].name = "comment";
	state->state[0] = 0; /* state[0] = we found a mask */
	return CSV_CONTINUE;
}

static int netcsv_validate_header(struct csv_field *field)
{
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

int load_netcsv_file(char *name, struct subnet_file *sf, struct st_options *nof)
{
	/* default netcsv fields */
	struct csv_field csv_field[] = {
		{ "prefix"	, 0, 1, 1, &netcsv_prefix_handle },
		{ "mask"	, 0, 2, 0, &netcsv_mask_handle },
		{ "device"	, 0, 0, 0, &netcsv_device_handle },
		{ "GW"		, 0, 3, 0, &netcsv_GW_handle },
		{ "comment"	, 0, 4, 0, &netcsv_comment_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;
	int res;

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

	init_csv_file(&cf, name, csv_field, nof->delim, &simple_strtok_r);
	cf.is_header = &netcsv_is_header;
	cf.endofline_callback = &netcsv_endofline_callback;
	cf.validate_header = &netcsv_validate_header;
	init_csv_state(&state, name);

	if (alloc_subnet_file(sf, 4096) < 0)
		return -2;
	zero_route(&sf->routes[0]);
	res = alloc_route_ea(&sf->routes[0], 1);
	sf->routes[0].ea[0].name = "comment";
	if (res < 0) {
		free_subnet_file(sf);
		return res;
	}
	res = generic_load_csv(name, &cf, &state, sf);
	if (res < 0) {
		free_subnet_file(sf);
		return res;
	}
	/* we allocated one more route */
	free_route(&sf->routes[sf->nr]);
	return res;
}

static int ipam_comment_handle(char *s, void *data, struct csv_state *state)
{
        struct  subnet_file *sf = data;

	if (strlen(s) > 2)/* sometimes comment are fucked and a better one is in EA-Name */
		ea_strdup(&sf->routes[sf->nr].ea[0], s);
	return CSV_VALID_FIELD;
}

/*
 * legacy IPAM loading function
 */
int load_ipam_no_EA(char  *name, struct subnet_file *sf, struct st_options *nof)
{
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
	int res;

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
	init_csv_file(&cf, name, csv_field, nof->ipam_delim, &simple_strtok_r);
        cf.is_header = netcsv_is_header;
	cf.endofline_callback = netcsv_endofline_callback;
	init_csv_state(&state, name);

	if (alloc_subnet_file(sf, 16192) < 0)
		return -2;
	zero_route(&sf->routes[0]);
	res = alloc_route_ea(&sf->routes[0], 1);
	if (res < 0) {
		free_subnet_file(sf);
		return res;
	}
	res = generic_load_csv(name, &cf, &state, sf);
	if (res < 0) {
		free_subnet_file(sf);
		return res;
	}
	/* we allocated one more route */
	free_route(&sf->routes[sf->nr]);
	return res;
}

int alloc_bgp_file(struct bgp_file *sf, unsigned long n)
{
	if (n > SIZE_T_MAX / sizeof(struct bgp_route)) { /* being paranoid */
		fprintf(stderr, "error: too much memory requested for struct route\n");
		return -1;
	}
	sf->routes = st_malloc(sizeof(struct bgp_route) * n, "bgp_file");
	if (sf->routes == NULL) {
		sf->nr = sf->max_nr = 0;
		return -1;
	}
	sf->nr     = 0;
	sf->max_nr = n;
	return 0;
}

void free_bgp_file(struct bgp_file *sf)
{
	free(sf->routes);
	total_memory -= sf->max_nr * sizeof(struct bgp_route);
	sf->routes = NULL;
	sf->nr = sf->max_nr = 0;
}

static int bgpcsv_prefix_handle(char *s, void *data, struct csv_state *state)
{
	struct bgp_file *sf = data;
	int res, i = 0;
	struct subnet subnet;

	while (isspace(s[i]))
		i++;
	res = get_subnet_or_ip(s + i, &subnet);
	if (res < 0) {
		debug(LOAD_CSV, 2, "invalid IP %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	copy_subnet(&sf->routes[sf->nr].subnet,  &subnet);
	return CSV_VALID_FIELD;
}

static int bgpcsv_GW_handle(char *s, void *data, struct csv_state *state)
{
	struct bgp_file *sf = data;
	struct ip_addr addr;
	int res, i = 0;

	while (isspace(s[i]))
		i++;
	res = string2addr(s + i, &addr, 41);
	if (res == sf->routes[sf->nr].subnet.ip_ver) {/* does the gw have same IPversion*/
		copy_ipaddr(&sf->routes[sf->nr].gw, &addr);
	} else {
		zero_ipaddr(&sf->routes[sf->nr].gw);
		debug(LOAD_CSV, 2, "invalid GW %s line %lu\n", s, state->line);
	}
	return CSV_VALID_FIELD;
}

static int bgpcsv_med_handle(char *s, void *data, struct csv_state *state)
{
	struct bgp_file *sf = data;
	int res = 0;
	int i = 0;

	while (isspace(s[i]))
		i++;
	while (isdigit(s[i])) {
		res *= 10;
		res += s[i] - '0';
		i++;
	}
	if (s[i] != '\0') {
                debug(LOAD_CSV, 2, "line %lu MED '%s' is not an INT\n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].MED = res;
	return CSV_VALID_FIELD;
}

static int bgpcsv_localpref_handle(char *s, void *data, struct csv_state *state)
{
	struct bgp_file *sf = data;
	int res = 0;
	int i = 0;

	while (isspace(s[i]))
		i++;
	while (isdigit(s[i])) {
		res *= 10;
		res += s[i] - '0';
		i++;
	}
	if (s[i] != '\0') {
                debug(LOAD_CSV, 2, "line %lu LOCAL_PREF '%s' is not an INT\n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].LOCAL_PREF = res;
	return CSV_VALID_FIELD;
}

static int bgpcsv_weight_handle(char *s, void *data, struct csv_state *state)
{
	struct bgp_file *sf = data;
	int res = 0;
	int i = 0;

	while (isspace(s[i]))
		i++;
	while (isdigit(s[i])) {
		res *= 10;
		res += s[i] - '0';
		i++;
	}
	if (s[i] != '\0') {
                debug(LOAD_CSV, 2, "line %lu WEIGHT '%s' is not an INT\n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].weight = res;
	return CSV_VALID_FIELD;
}

static int bgpcsv_aspath_handle(char *s, void *data, struct csv_state *state)
{
	struct bgp_file *sf = data;
	int res;

	res = strxcpy(sf->routes[sf->nr].AS_PATH, s, sizeof(sf->routes[sf->nr].AS_PATH));
	if (res >= sizeof(sf->routes[sf->nr].AS_PATH))
		debug(LOAD_CSV, 1, "line %lu STRING AS_PATH '%s'  too long, truncating to '%s'\n", state->line, s, sf->routes[sf->nr].AS_PATH);
	return CSV_VALID_FIELD;
}

static int bgpcsv_best_handle(char *s, void *data, struct csv_state *state)
{
	struct bgp_file *sf = data;

	while (isspace(*s))
		s++;
	if (!strcasecmp(s, "BEST"))
		sf->routes[sf->nr].best = 1;
	else
		sf->routes[sf->nr].best = 0;
	return CSV_VALID_FIELD;
}

static int bgpcsv_type_handle(char *s, void *data, struct csv_state *state)
{
	struct bgp_file *sf = data;

	while (isspace(*s))
		s++;
	if (!strcasecmp(s, "ebgp")) /* FIXME */
		sf->routes[sf->nr].type = 'e';
	else if (!strcasecmp(s, "ibgp"))
		sf->routes[sf->nr].type = 'i';
	return CSV_VALID_FIELD;
}

static int bgpcsv_origin_handle(char *s, void *data, struct csv_state *state)
{
	struct bgp_file *sf = data;
	int i = 0;

	while (isspace(s[i]))
		i++;
	if (s[i] == 'e' || s[i] == 'i' || s[i] == '?') {
		sf->routes[sf->nr].origin = s[i];
		return CSV_VALID_FIELD;
	} else {
                debug(LOAD_CSV, 2, "line %lu ORIGIN CODE '%c' is invalid\n", state->line, s[i]);
		return CSV_INVALID_FIELD_BREAK;
	}
}

static int bgpcsv_valid_handle(char *s, void *data, struct csv_state *state)
{
	struct bgp_file *sf = data;

	if (!strcmp(s, "1"))
		sf->routes[sf->nr].valid = 1;
	else
		sf->routes[sf->nr].valid = 0;
	return CSV_VALID_FIELD;
}

static int bgpcsv_endofline_callback(struct csv_state *state, void *data)
{
	struct bgp_file *sf = data;
	struct bgp_route *new_r;

	if (state->badline) {
		debug(LOAD_CSV, 1, "%s : invalid line %lu\n", state->file_name, state->line);
		return -1;
	}
	sf->nr++;
	if  (sf->nr == sf->max_nr) {
		if (sf->max_nr * 2 > SIZE_T_MAX / sizeof(struct bgp_route)) {
			fprintf(stderr, "error: too much memory requested for struct route\n");
			return CSV_CATASTROPHIC_FAILURE;
		}
		new_r = st_realloc(sf->routes, sizeof(struct bgp_route) * sf->max_nr * 2,
				sizeof(struct bgp_route) * sf->max_nr,
				"bgp_route");
		if (new_r == NULL)
			return  CSV_CATASTROPHIC_FAILURE;
		sf->max_nr *= 2;
		sf->routes = new_r;
	}
	zero_bgproute(&sf->routes[sf->nr]);
	return CSV_CONTINUE;
}

static int bgp_field_compare(const char *s1, const char *s2)
{
	int i = 0;

	while (isspace(s1[i]))
		i++;
	return strcmp(s1 + i, s2);
}

int load_bgpcsv(char  *name, struct bgp_file *sf, struct st_options *nof)
{
	struct csv_field csv_field[] = {
		{ "prefix"	, 0, 0, 1, &bgpcsv_prefix_handle },
		{ "GW"		, 0, 0, 1, &bgpcsv_GW_handle },
		{ "LOCAL_PREF"	, 0, 0, 1, &bgpcsv_localpref_handle },
		{ "MED"		, 0, 0, 1, &bgpcsv_med_handle },
		{ "WEIGHT"	, 0, 0, 1, &bgpcsv_weight_handle },
		{ "AS_PATH"	, 0, 0, 0, &bgpcsv_aspath_handle },
		{ "BEST"	, 0, 0, 1, &bgpcsv_best_handle },
		{ "ORIGIN"	, 0, 0, 1, &bgpcsv_origin_handle },
		{ "V"		, 0, 0, 1, &bgpcsv_valid_handle },
		{ "Proto"	, 0, 0, 1, &bgpcsv_type_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;

        cf.is_header = NULL;
	init_csv_file(&cf, name, csv_field, nof->delim, &strtok_r);
	cf.endofline_callback   = bgpcsv_endofline_callback;
	cf.header_field_compare = bgp_field_compare;
	init_csv_state(&state, name);

	if (alloc_bgp_file(sf, 16192) < 0)
		return -2;
	zero_bgproute(&sf->routes[0]);
	return generic_load_csv(name, &cf, &state, sf);
}
