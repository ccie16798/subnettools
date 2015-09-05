/*
 * routes alloc functions
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
#include "st_routes.h"

inline void copy_route(struct route *a, const struct route *b) {
	memcpy(a, b, sizeof(struct route));
}

inline void zero_route(struct route *a) {
	memset(a, 0, sizeof(struct route));
}

void zero_route_ea(struct route *a) {
	int i;
	void *ea = a->ea;
	int ea_nr = a->ea_nr;

	for (i = 0; i < ea_nr; i++)
		a->ea[i].value[0] = '\0';
	zero_route(a);
	a->ea = ea;
	a->ea_nr = ea_nr;
}

int alloc_route_ea(struct route *r, int n) {
	int i;

	r->ea = st_malloc(n * sizeof(struct ipam_ea), "route ea");
	r->ea_nr = n;
	if (r->ea == NULL) {
		r->ea_nr = 0;
		return -1;
	}
	for (i = 0; i < n; i++)
		r->ea[i].value = NULL;
	return 1;
}

int clone_route(struct route *dest, const struct route *src) {
	int i, res;

	copy_route(dest, src);
	res = alloc_route_ea(dest, src->ea_nr);
	if (res < 0)
		return res;
	for (i = 0; i <	dest->ea_nr; i++) {
		/* name IS not malloc'ed, only value */
		dest->ea[i].name  = src->ea[i].name;
		if (src->ea[i].value) {
			dest->ea[i].value = st_strdup(src->ea[i].value);
			if (dest->ea[i].value == NULL)
				return -1;
		}
		else
			dest->ea[i].value = NULL;
	}
	return 1;
}

void free_route(struct route *r) {
	int i;

	for (i = 0; i < r->ea_nr; i++) {
		total_memory -= ea_size(&r->ea[i]);
		free(r->ea[i].value);
		r->ea[i].value = NULL;
	}
	free(r->ea);
	total_memory -= sizeof(struct ipam_ea) * r->ea_nr;
	r->ea = NULL;
	r->ea_nr = 0;
}

int is_equal_gw(struct route *r1, struct route *r2) {
	if (r1->gw.ip_ver != r2->gw.ip_ver)
		return 0;
	if (r1->gw.ip_ver == 0) /* unset GW*/
		return 1;
	if (r1->gw.ip_ver == IPV4_A && r1->gw.ip == r2->gw.ip)
		return 1;
	else if (r1->gw.ip_ver == IPV6_A) {
		if (!is_equal_ipv6(r1->gw.ip6, r2->gw.ip6))
			return 0;
		if (!ipv6_is_link_local(r1->gw.ip6))
			return 1;
		/* if link local adress, we must check if the device is the same */
		return !strcmp(r1->device, r2->device);
	}
	return 0;
}
