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
#include "st_memory.h"
#include "iptools.h"
#include "utils.h"
#include "generic_csv.h"
#include "heap.h"
#include "st_printf.h"
#include "st_scanf.h"
#include "generic_expr.h"
#include "bgp_tool.h"

#define SIZE_T_MAX ((size_t)0 - 1)

int fprint_bgp_route(FILE *output, struct bgp_route *route)
{
	return st_fprintf(output, "%d;%s;%s;%16P;%16a;%10d;%10d;%10d;     %c;%s\n",
			route->valid,
			(route->type == 'i' ? " iBGP" : " eBGP"),
			(route->best == 1 ? "Best" : "  No"),
			route->subnet, route->gw, route->MED,
			route->LOCAL_PREF, route->weight,
			route->origin,
			route->AS_PATH);
}

void fprint_bgp_file(FILE *output, struct bgp_file *bf)
{
	unsigned long i = 0;

	for (i = 0; i < bf->nr; i++)
		fprint_bgp_route(output, &bf->routes[i]);
}

void fprint_bgp_file_header(FILE *out)
{
	fprintf(out, "V;Proto;BEST;          prefix;              GW;       MED;LOCAL_PREF;    WEIGHT;ORIGIN;AS_PATH;\n");
}

void copy_bgproute(struct bgp_route *a, const struct bgp_route *b)
{
	memcpy(a, b, sizeof(struct bgp_route));
}

void zero_bgproute(struct bgp_route *a)
{
	memset(a, 0, sizeof(struct bgp_route));
}

int compare_bgp_file(const struct bgp_file *sf1, const struct bgp_file *sf2, struct st_options *o)
{
	unsigned long i, j;
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
			if (sf1->routes[i].origin != sf2->routes[j].origin)
				changed++;
			if (strcmp(sf1->routes[i].AS_PATH, sf2->routes[j].AS_PATH))
				changed++;

			if (changed)
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

int as_path_length(const char *s)
{
	int i = 0;
	int num = 0;
	int in_confed = 0;
	int in_asset = 0;
	char c;

	while (isspace(s[i]))
		i++;
	if (s[i] == '\0')
		return 0;
	for ( ; i < strlen(s); i++) {
		c = s[i];
		if (in_confed && (c == '{' || c == '}' || c == '(')) {
			st_debug(BGPCMP, 2, "BAD AS_PATH '%s'\n", s);
			return -1;
		}
		if (in_asset && (c == '(' || c == ')' || c == '{')) {
			st_debug(BGPCMP, 2, "BAD AS_PATH '%s'\n", s);
			return -1;
		}
		if (c == '(')
			in_confed = 1;
		else if (c == '{')
			in_asset = 1;
		else if (c == ')' && in_confed) {
			in_confed = 0;
			continue;
		}
		else if (c == '}' && in_asset) {
			in_asset = 0;
			continue;
		}
		/* AS in AS_CONFED or AS_SET doesnt count int the AS_PATH length */
		if (in_asset || in_confed)
			continue;
		if  (isdigit(s[i]) && (isspace(s[i + 1]) || s[i + 1] == '\0'))
			num++;
	}
	if (in_asset || in_confed) {
		st_debug(BGPCMP, 2, "BAD AS_PATH '%s'\n", s);
		return -1;
	}
	return num;
}

static int __heap_subnet_is_superior(void *v1, void *v2)
{
	struct subnet *s1 = &((struct bgp_route *)v1)->subnet;
	struct subnet *s2 = &((struct bgp_route *)v2)->subnet;

	return subnet_is_superior(s1, s2);
}

static int __heap_gw_is_superior(void *v1, void *v2)
{
	struct subnet *s1 = &((struct bgp_route *)v1)->subnet;
	struct subnet *s2 = &((struct bgp_route *)v2)->subnet;
	struct ip_addr *gw1 = &((struct bgp_route *)v1)->gw;
	struct ip_addr *gw2 = &((struct bgp_route *)v2)->gw;

	if (is_equal_ip(gw1, gw2))
		return subnet_is_superior(s1, s2);
	else
		return addr_is_superior(gw1, gw2);
}

static int __heap_med_is_superior(void *v1, void *v2)
{
	struct subnet *s1 = &((struct bgp_route *)v1)->subnet;
	struct subnet *s2 = &((struct bgp_route *)v2)->subnet;
	int MED1 = ((struct bgp_route *)v1)->MED;
	int MED2 = ((struct bgp_route *)v2)->MED;

	if (MED1 == MED2)
		return subnet_is_superior(s1, s2);
	else
		return (MED1 < MED2);
}

static int __heap_mask_is_superior(void *v1, void *v2)
{
	struct subnet *s1 = &((struct bgp_route *)v1)->subnet;
	struct subnet *s2 = &((struct bgp_route *)v2)->subnet;

	if (s1->mask == s2->mask)
		return subnet_is_superior(s1, s2);
	else
		return (s1->mask < s2->mask);
}

static int __heap_localpref_is_superior(void *v1, void *v2)
{
	struct subnet *s1 = &((struct bgp_route *)v1)->subnet;
	struct subnet *s2 = &((struct bgp_route *)v2)->subnet;
	int LOCAL_PREF1 = ((struct bgp_route *)v1)->LOCAL_PREF;
	int LOCAL_PREF2 = ((struct bgp_route *)v2)->LOCAL_PREF;

	if (LOCAL_PREF1 == LOCAL_PREF2) {
		return subnet_is_superior(s1, s2);
	} else
		return (LOCAL_PREF1 > LOCAL_PREF2);
}

static int __heap_aspath_is_superior(void *v1, void *v2)
{
	char *s1 = ((struct bgp_route *)v1)->AS_PATH;
	char *s2 = ((struct bgp_route *)v2)->AS_PATH;
	int l1, l2;

	/* if AS_PATH length is the same
	 * we compare char by char the strings
	 * if AS_PATh is the same, we sort by prefix
	 */
	l1 = as_path_length(s1);
	l2 = as_path_length(s2);
	if (l1 == l2) {
		if (!strcmp(s1,s2)) {
			struct subnet *sub1 = &((struct bgp_route *)v1)->subnet;
			struct subnet *sub2 = &((struct bgp_route *)v2)->subnet;
			return subnet_is_superior(sub1, sub2);
		}
		return strcmp(s1, s2);
	}
	return (l1 < l2);
}


static int __bgp_sort_by(struct bgp_file *sf, int cmpfunc(void *v1, void *v2))
{
	unsigned long i;
	TAS tas;
	struct bgp_route *new_r, *r;
	int res;

	if (sf->nr == 0)
		return 0;
	res = alloc_tas(&tas, sf->nr, cmpfunc);
	if (res < 0)
		return res;

	new_r = st_malloc(sf->max_nr * sizeof(struct bgp_route), "new bgp_route");
	if (new_r == NULL) {
		free_tas(&tas);
		return -1;
	}
	/* basic heapsort */
	for (i = 0 ; i < sf->nr; i++)
		addTAS(&tas, &(sf->routes[i]));
	for (i = 0 ; i < sf->nr; i++) {
		r = popTAS(&tas);
		copy_bgproute(&new_r[i], r);
	}
	free_tas(&tas);
	st_free(sf->routes, sf->max_nr * sizeof(struct bgp_route));
	sf->routes = new_r;
	return 0;
}

struct bgpsort {
	char *name;
	int (*cmpfunc)(void *v1, void *v2);
};

static const struct bgpsort bgpsort[] = {
	{ "prefix",	&__heap_subnet_is_superior },
	{ "gw",		&__heap_gw_is_superior },
	{ "med",	&__heap_med_is_superior },
	{ "mask",	&__heap_mask_is_superior },
	{ "localpref",	&__heap_localpref_is_superior },
	{ "aspath",	&__heap_aspath_is_superior },
	{NULL,		NULL}
};

void bgp_available_cmpfunc(FILE *out)
{
	int i = 0;

	while (1) {
		if (bgpsort[i].name == NULL)
			break;
		fprintf(out, " %s\n", bgpsort[i].name);
		i++;
	}
}

int bgp_sort_by(struct bgp_file *sf, char *name)
{
	int i = 0, res;

	while (1) {
		if (bgpsort[i].name == NULL)
			break;
		if (!strncasecmp(name, bgpsort[i].name, strlen(name))) {
			debug_timing_start(2);
			res = __bgp_sort_by(sf, bgpsort[i].cmpfunc);
			debug_timing_end(2);
			return res;
		}
		i++;
	}
	return -1664;
}

#define BLOCK_INT(__VAR) \
		if (res < 0) \
			return 0; \
		switch (op) { \
		case '=': \
			return route->__VAR == res; \
			break; \
		case '#': \
			return route->__VAR != res; \
			break; \
		case '<': \
			return route->__VAR < res; \
			break; \
		case '>': \
			return route->__VAR > res; \
			break; \
		default: \
			debug(FILTER, 1, "Unsupported op '%c' for %s\n", op, #__VAR); \
			return -1; \
		} \

int fprint_bgpfilter_help(FILE *out)
{

	return fprintf(out, "BGP routes can be filtered on :\n"
			" -prefix\n"
			" -mask\n"
			" -gw\n"
			" -LOCAL_PREF, MED, weight\n"
			" -Valid, Best\n"
			" -AS_PATH (=, <, > and # compare AS_PATH length; to compare actual AS_PATH, use '~')\n\n"
			"operator are :\n"
			"- '=' (EQUALS)\n"
			"- '#' (DIFFERENT)\n"
			"- '<' (numerically inferior)\n"
			"- '>' (numerically superior)\n"
			"- '{' (is included (for prefixes))\n"
			"- '}' (includes (for prefixes))\n"
			"- '~' (st_scanf regular expression)\n");
}


static int bgp_route_filter(char *s, char *value, char op, void *object)
{
	struct bgp_route *route = object;
	struct subnet subnet;
	int res;
	int err;

	debug(FILTER, 8, "Filtering '%s' %c '%s'\n", s, op, value);
	if (!strcmp(s, "prefix")) {
		res = get_subnet_or_ip(value, &subnet);
		if (res < 0) {
			debug(FILTER, 1, "Filtering on prefix %c '%s',  but it is not an IP\n", op, value);
			return -1;
		}
		return subnet_filter(&route->subnet, &subnet, op);
	}
	else if (!strcmp(s, "gw")) {
		if (route->gw.ip_ver == 0)
			return 0;
		res = get_subnet_or_ip(value, &subnet);
		if (res < 0) {
			debug(FILTER, 1, "Filtering on gw %c '%s',  but it is not an IP\n", op, value);
			return -1;
		}
		switch (op) {
		case '=':
			return is_equal_ip(&route->gw, &subnet.ip_addr);
			break;
		case '#':
			return !is_equal_ip(&route->gw, &subnet.ip_addr);
			break;
		case '<':
			return addr_is_superior(&route->gw, &subnet.ip_addr);
			break;
		case '>':
			return (!addr_is_superior(&route->gw, &subnet.ip_addr) &&
					!is_equal_ip(&route->gw, &subnet.ip_addr));
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for prefix\n", op);
			return 0;
		}
	}
	else if (!strcmp(s, "mask")) {
		res =  string2mask(value, 42);
		if (res < 0) {
			debug(FILTER, 1, "Filtering on mask %c '%s',  but it is valid\n", op, value);
			return -1;
		}
		BLOCK_INT(subnet.mask);
	}
	else if (!strcasecmp(s, "med")) {
		res =  string2int(value, &err);
		if (err < 0) {
			debug(FILTER, 1, "Filtering on MED %c '%s',  but it is not valid \n", op, value);
			return -1;
		}
		BLOCK_INT(MED);
	}
	else if (!strcasecmp(s, "weight")) {
		res =  string2int(value, &err);
		if (err < 0) {
			debug(FILTER, 1, "Filtering on WEIGHT %c '%s',  but it is not valid \n", op, value);
			return -1;
		}
		BLOCK_INT(weight);
	}
	else if (!strcasecmp(s, "LOCALPREF") || !strcasecmp(s, "local_pref")) {
		res =  string2int(value, &err);
		if (err < 0) {
			debug(FILTER, 1, "Filtering on LOCAL_PREF %c '%s',  but it is not valid \n", op, value);
			return -1;
		}
		BLOCK_INT(LOCAL_PREF);
	}
	/* we compare AS_PATH length, except with ~ compator
	 * that comparator uses pattern matching */
	else if (!strcasecmp(s, "aspath") || !strcasecmp(s, "as_path")) {
		if (op == '~') {
			res = st_sscanf(route->AS_PATH, value);
			return (res < 0 ? 0 : 1);
		}
		res =  string2int(value, &err);
		if (err < 0) {
			debug(FILTER, 1, "Filtering on AS_PATH length %c '%s',  but it is not valid \n", op, value);
			return -1;
		}
		switch (op) {
		case '=':
			return (as_path_length(route->AS_PATH) == res);
			break;
		case '#':
			return (as_path_length(route->AS_PATH) != res);
			break;
		case '<':
			return (as_path_length(route->AS_PATH) < res);
			break;
		case '>':
			return (as_path_length(route->AS_PATH) > res);
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for AS_PATH\n", op);
			return -1;
			break;
		}
	}
	else if (!strcasecmp(s, "best")) {
		res =  string2int(value, &err);
		if (err < 0) {
			debug(FILTER, 1, "Filtering on Best %c '%s',  but it is not valid \n", op, value);
			return -1;
		}
		switch (op) {
		case '=':
			return route->best == res;
			break;
		case '#':
			return route->best != res;
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for best\n", op);
			return -1;
			break;
		}
	}
	else if (!strcasecmp(s, "type")) {
		switch (op) {
		case '=':
			return route->type == *value;
			break;
		case '#':
			return route->type != *value;
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for type\n", op);
			return -1;
		}
	}
	else if (!strcasecmp(s, "origin")) {
		switch (op) {
		case '=':
			return route->origin == *value;
			break;
		case '#':
			return route->origin != *value;
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for origin\n", op);
			return -1;
		}
	}
	debug(FILTER, 1, "Cannot filter on attribute '%s'\n", s);
	return 0;
}

int bgp_file_filter(struct bgp_file *sf, char *expr)
{
	unsigned long i, j;
	int res, len;
	struct generic_expr e;
	struct bgp_route *new_r;

	if (sf->nr == 0)
		return 0;
	init_generic_expr(&e, expr, bgp_route_filter);
	debug_timing_start(2);

	new_r = st_malloc(sf->max_nr * sizeof(struct bgp_route), "bgp_route");
	if (new_r == NULL) {
		debug_timing_end(2);
		return -1;
	}
	j = 0;
	len = strlen(expr);

	for (i = 0; i < sf->nr; i++) {
		e.object = &sf->routes[i];
		res = run_generic_expr(expr, len, &e);
		if (res < 0) {
			fprintf(stderr, "Invalid filter '%s'\n", expr);
			st_free(new_r, sf->max_nr * sizeof(struct bgp_route));
			debug_timing_end(2);
			return -1;
		}
		if (res) {
			st_debug(FILTER, 5, "Matching filter '%s' on %P\n", expr, sf->routes[i].subnet);
			copy_bgproute(&new_r[j], &sf->routes[i]);
			j++;
		}
	}
	st_free(sf->routes, sf->max_nr * sizeof(struct bgp_route));
	sf->routes = new_r;
	sf->nr = j;
	debug_timing_end(2);
	return 0;
}
