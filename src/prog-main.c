/*
 * subnet tools MAIN
 *
 * Copyright (C) 2014, 2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
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
#include <sys/types.h>
#include <sys/resource.h>
#include "iptools.h"
#include "string2ip.h"
#include "st_routes.h"
#include "routetocsv.h"
#include "utils.h"
#include "generic_csv.h"
#include "generic_command.h"
#include "config_file.h"
#include "st_printf.h"
#include "st_scanf.h"
#include "ipinfo.h"
#include "generic_expr.h"
#include "st_routes_csv.h"
#include "subnet_tool.h"
#include "bgp_tool.h"
#include "ipam.h"
#include "st_memory.h"
#include "st_help.h"
#include "prog-main.h"

/* max number of objects collectable inf fscanf, and scanf */
#define SCANF_MAX_OBJECTS 40
const char *default_fmt       = "%I;%m;%D;%G;%O#";
const char *bgp_default_fmt   = "%v;%5T;%4B;%16P;%16G;%10M;%10L;%10w;%6o;%A";
const char *ipam_default_fmt  = "%I;%m";
/*
 * scanf default format; it prints 9 arguments, separated with spaces
 * please run 'scanf' with -fmt "%O0, %O1 etc..." option to choose appropriate
 * output format
 */
const char *scanf_default_fmt = "%O0 %O1 %O2 %O3 %O4 %O5 %O6 %O7\n";

/* struct file_options and MACROs ffrom config_file.[ch] */
struct file_options fileoptions[] = {
	{ FILEOPT_LINE(ipam_prefix_field, struct st_options, TYPE_STRING),
		"IPAM CSV header field describing the prefix"  },
	{ FILEOPT_LINE(ipam_mask, struct st_options, TYPE_STRING),
		"IPAM CSV header field describing the mask" },
	{ FILEOPT_LINE(ipam_comment1, struct st_options, TYPE_STRING),
		"IPAM CSV header field describing comment" },
	{ FILEOPT_LINE(ipam_comment2, struct st_options, TYPE_STRING),
		"IPAM CSV header field describing comment" },
	{ FILEOPT_LINE(ipam_delim, struct st_options, TYPE_STRING),  "IPAM CSV delimitor" },
	{ FILEOPT_LINE(ipam_ea, struct st_options, TYPE_STRING),
		"IPAM Extended Attributes to collect" },
	{ "netcsv_delim", TYPE_STRING, sizeofmember(struct st_options, delim),
		 offsetof(struct st_options, delim), "CSV delimitor" },
	{ FILEOPT_LINE(netcsv_prefix_field, struct st_options, TYPE_STRING),
		"Subnet CSV header field describing the prefix" },
	{ FILEOPT_LINE(netcsv_mask, struct st_options, TYPE_STRING),
		"Subnet CSV header field describing the mask" },
	{ FILEOPT_LINE(netcsv_comment, struct st_options, TYPE_STRING),
		"Subnet CSV header field describing the comment" },
	{ FILEOPT_LINE(netcsv_device, struct st_options, TYPE_STRING),
		"Subnet CSV header field describing the device" },
	{ FILEOPT_LINE(netcsv_gw, struct st_options, TYPE_STRING),
		"Subnet CSV header field describing the gateway" },
	{ FILEOPT_LINE(output_fmt, struct st_options, TYPE_STRING),
		"Default Output Format String" },
	{ FILEOPT_LINE(bgp_output_fmt, struct st_options, TYPE_STRING),
		"Default BGP Output Format String" },
	{ FILEOPT_LINE(ipam_output_fmt, struct st_options, TYPE_STRING),
		"Default IPAM Output Format String" },
	{ FILEOPT_LINE(subnet_off, struct st_options, TYPE_INT) },
	{NULL,                  0, 0}
};


static int run_compare(int argc, char **argv, void *st_options);
static int run_subnetcmp(int argc, char **argv, void *st_options);
static int run_missing(int argc, char **argv, void *st_options);
static int run_uniq(int argc, char **argv, void *st_options);
static int run_paip(int argc, char **argv, void *st_options);
static int run_ipam_getea(int argc, char **argv, void *st_options);
static int run_grep(int argc, char **argv, void *st_options);
static int run_convert(int argc, char **argv, void *st_options);
static int run_routesimplify1(int argc, char **argv, void *st_options);
static int run_routesimplify2(int argc, char **argv, void *st_options);
static int run_common(int argc, char **argv, void *st_options);
static int run_addfiles(int argc, char **argv, void *st_options);
static int run_sort(int argc, char **argv, void *st_options);
static int run_sortby(int argc, char **argv, void *st_options);
static int run_filter(int argc, char **argv, void *st_options);
static int run_ipam_filter(int argc, char **argv, void *st_options);
static int run_bgp_filter(int argc, char **argv, void *st_options);
static int run_sum(int argc, char **argv, void *st_options);
static int run_scanf(int argc, char **argv, void *st_options);
static int run_fscanf(int argc, char **argv, void *st_options);
static int run_subnetagg(int argc, char **argv, void *st_options);
static int run_routeagg(int argc, char **argv, void *st_options);
static int run_remove(int argc, char **argv, void *st_options);
static int run_remove_file(int argc, char **argv, void *st_options);
static int run_split(int argc, char **argv, void *st_options);
static int run_split_2(int argc, char **argv, void *st_options);
static int run_help(int argc, char **argv, void *st_options);
static int run_version(int argc, char **argv, void *st_options);
static int run_confdesc(int argc, char **argv, void *st_options);
static int run_relation(int argc, char **argv, void *st_options);
static int run_ipinfo(int argc, char **argv, void *st_options);
static int run_bgpcmp(int argc, char **argv, void *st_options);
static int run_bgpsortby(int argc, char **argv, void *st_options);
static int run_echo(int argc, char **argv, void *st_options);
static int run_print(int argc, char **argv, void *st_options);
static int run_bgpprint(int argc, char **argv, void *st_options);
static int run_ipamprint(int argc, char **argv, void *st_options);
static int run_test(int argc, char **argv, void *st_options);
static int run_gen_expr(int argc, char **argv, void *st_options);
static int run_test2(int argc, char **argv, void *st_options);

static int option_verbose(int argc, char **argv, void *st_options);
static int option_verbose2(int argc, char **argv, void *st_options);
static int option_delim(int argc, char **argv, void *st_options);
static int option_ipam_ea(int argc, char **argv, void *st_options);
static int option_grepfield(int argc, char **argv, void *st_options);
static int option_output(int argc, char **argv, void *st_options);
static int option_debug(int argc, char **argv, void *st_options);
static int option_config(int argc, char **argv, void *st_options);
static int option_addr_compress(int argc, char **argv, void *st_options);
static int option_fmt(int argc, char **argv, void *st_options);
static int option_rt(int argc, char **argv, void *st_options);
static int option_ecmp(int argc, char **argv, void *st_options);
static int option_noheader(int argc, char **argv, void *st_options);

struct st_command commands[] = {
	{ "echo",		&run_echo,	2},
	{ "print",		&run_print,	0},
	{ "bgpprint",		&run_bgpprint,	0},
	{ "ipamprint",		&run_ipamprint,	0},
	{ "relation",		&run_relation,	2},
	{ "bgpcmp",		&run_bgpcmp,	2},
	{ "bgpsortby",		&run_bgpsortby,	1},
	{ "ipinfo",		&run_ipinfo,	1},
	{ "compare",		&run_compare,	2},
	{ "subnetcmp",		&run_subnetcmp,	2},
	{ "missing",		&run_missing,	2},
	{ "uniq",		&run_uniq,	2},
	{ "paip",		&run_paip,	1},
	{ "ipam",		&run_paip,	1},
	{ "getea",		&run_ipam_getea, 1},
	{ "grep",		&run_grep,	2},
	{ "convert",		&run_convert,	1},
	{ "routesimplify1",	&run_routesimplify1,	1},
	{ "routesimplify2",	&run_routesimplify2,	1},
	{ "common",		&run_common,	2},
	{ "addfiles",		&run_addfiles,	2},
	{ "sort",		&run_sort,	0},
	{ "sortby",		&run_sortby,	1},
	{ "filter",		&run_filter,	1},
	{ "ipamfilter",		&run_ipam_filter, 1},
	{ "bgpfilter",		&run_bgp_filter, 1},
	{ "sum",		&run_sum,	1},
	{ "subnetagg",		&run_subnetagg,	1},
	{ "routeagg",		&run_routeagg,	1},
	{ "removesubnet",	&run_remove,	3},
	{ "removefile",		&run_remove_file, 2},
	{ "split",		&run_split,	2},
	{ "split2",		&run_split_2,	2},
	{ "scanf",		&run_scanf,	2},
	{ "fscanf",		&run_fscanf,	2},
	{ "help",		&run_help,	0},
	{ "version",		&run_version,	0},
	{ "confdesc",		&run_confdesc,	0},
	/* 'hidden' debug functions */
	{ "exprtest",	&run_gen_expr,	1, 1},
	{ "test",	&run_test,	1, 1},
	{ "test2",	&run_test2,	2, 1},
	{ NULL,		NULL,		0, 0}
};

struct st_command options[] = {
	{"-V",		&option_verbose,	0},
	{"-VV",		&option_verbose2,	0},
	{"-d",		&option_delim,		1},
	{"-grep_field",	&option_grepfield,	1},
	{"-o",		&option_output,		1},
	{"-D",		&option_debug,		1},
	{"-c",		&option_config,		1},
	{"-p",		&option_addr_compress,	1},
	{"-fmt",	&option_fmt,		1},
	{"-rt",		&option_rt,		0},
	{"-ecmp",	&option_ecmp,		0},
	{"-ipamea",	&option_ipam_ea,	1},
	{"-ea",		&option_ipam_ea,	1},
	{"-EA",		&option_ipam_ea,	1},
	{"-noheader",	&option_noheader,	0},
	{"-nh",		&option_noheader,	0},
	{NULL, NULL, 0}
};

#define BAD_FILE(__truc) \
		fprintf(stderr, "Invalid file %s\n", (__truc ? __truc : "<stdin>"))

#define DIE_ON_BAD_FILE(__truc2) \
	do { \
		if (res < 0) { \
			BAD_FILE(__truc2); \
			return res; \
		} \
	} while (0)

/*
 * COMMAND HANDLERS
 */
static int run_relation(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet subnet1, subnet2;

	res = get_subnet_or_ip(argv[2], &subnet1);
	if (res < 0) {
		fprintf(stderr, "%s is not an IP\n", argv[2]);
		return 0;
	}
	res = get_subnet_or_ip(argv[3], &subnet2);
	if (res < 0) {
		fprintf(stderr, "%s is not an IP\n", argv[3]);
		return 0;
	}
	if (subnet1.ip_ver != subnet2.ip_ver) {
		fprintf(stderr, "IP version is different\n");
		return 0;
	}
	res = subnet_compare(&subnet1, &subnet2);
	switch (res) {
	case EQUALS:
		st_printf("%P equals %P (subnet address : %N)\n", subnet1, subnet2, subnet1);
		break;
	case INCLUDED:
		st_printf("%s is included in %s\n", argv[2], argv[3]);
		break;
	case INCLUDES:
		st_printf("%s includes %s\n", argv[2], argv[3]);
		break;
	default:
		st_printf("%s has no relation with %s\n", argv[2], argv[3]);
		break;
	}
	return 0;
}

static int run_echo(int argc, char **argv, void *st_options)
{
	int res, i;
	struct subnet subnet;
	char *s =  argv[2];

	res = get_subnet_or_ip(argv[3], &subnet);
	if (res < 0) {
		fprintf(stderr, "Invalid IP\n");
		return -1;
	}
	/* make sure no "%s" specifier is there, or bad thing will happen */
	for (i = 0; ; i++) {
		if (s[i] == '\0')
			break;
		if (s[i] == '%') {
			i++;
			if (s[i] == '-')
				i++;
			while (isdigit(s[i]))
				i++;
			if (s[i] == '\0') {
				fprintf(stderr, "Bad format string '%s'\n", s);
				return -1;
			}
			if (s[i] != 'I' && s[i] != 'a' && s[i] != 'P' &&
					tolower(s[i]) != 'm' && s[i] != 'B' && s[i] != 'N' &&
					s[i] != 'L' && s[i] != 'U') {
				fprintf(stderr,
						"Cannot use Conversion Specifier '%c' with echo\n",
						s[i]);
				return -1;
			}
		}
	}
	st_printf(argv[2], subnet, subnet, subnet);
	printf("\n");
	return 0;
}

static int run_ipinfo(int argc, char **argv, void *st_options)
{
	int res;
	struct st_options *nof = st_options;
	struct subnet subnet;

	if (!strcasecmp(argv[2], "all")) {
		fprint_ipv4_known_subnets(nof->output_file);
		fprint_ipv6_known_subnets(nof->output_file);
		return 0;
	} else if (!strcasecmp(argv[2], "ipv4")) {
		fprint_ipv4_known_subnets(nof->output_file);
		return 0;
	} else if (!strcasecmp(argv[2], "ipv6")) {
		fprint_ipv6_known_subnets(nof->output_file);
		return 0;
	}
	res = get_subnet_or_ip(argv[2], &subnet);
	if (res < 0) {
		fprintf(stderr, "Invalid IP %s\n", argv[2]);
		return -1;
	}
	fprint_ip_info(nof->output_file, &subnet);
	return 0;
}

static int run_print(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf, nof);
	DIE_ON_BAD_FILE(argv[2]);
	if (nof->print_header)
		fprint_route_header(nof->output_file, &sf.routes[0], nof->output_fmt);
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free_subnet_file(&sf);
	return 0;
}

static int run_bgpprint(int argc, char **argv, void *st_options)
{
	int res;
	struct bgp_file sf;
	struct st_options *nof = st_options;

	res = load_bgpcsv(argv[2], &sf, nof);
	DIE_ON_BAD_FILE(argv[2]);

	fprint_bgproute_fmt(nof->output_file, NULL, nof->bgp_output_fmt);
	fprint_bgp_file_fmt(nof->output_file, &sf, nof->bgp_output_fmt);
	free_bgp_file(&sf);
	return 0;
}

static int run_ipamprint(int argc, char **argv, void *st_options)
{
	int res;
	struct ipam_file sf;
	struct st_options *nof = st_options;

	res = load_ipam(argv[2], &sf, nof);
	DIE_ON_BAD_FILE(argv[2]);
	/* print fmt header just if user provided a fmt */
	fprint_ipam_header(nof->output_file, &sf.lines[0], nof->ipam_output_fmt);
	fprint_ipam_file_fmt(nof->output_file, &sf, nof->ipam_output_fmt);
	free_ipam_file(&sf);
	return 0;
}

static int run_compare(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf1, sf2;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	DIE_ON_BAD_FILE(argv[2]);
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0) {
		free_subnet_file(&sf1);
		BAD_FILE(argv[3]);
		return res;
	}
	compare_files(&sf1, &sf2, nof);
	free_subnet_file(&sf1);
	free_subnet_file(&sf2);
	return 0;
}

static int run_subnetcmp(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file before, after, sf;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &before, nof);
	DIE_ON_BAD_FILE(argv[2]);
	res = load_netcsv_file(argv[3], &after, nof);
	if (res < 0) {
		free_subnet_file(&before);
		BAD_FILE(argv[3]);
		return res;
	}
	res = subnet_file_cmp(&before, &after, &sf);
	if (res < 0) {
		free_subnet_file(&before);
		free_subnet_file(&after);
		free_subnet_file(&sf);
		return res;
	}
	if (nof->print_header)
		fprint_route_header(nof->output_file, &sf.routes[0], nof->output_fmt);
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free_subnet_file(&before);
	free_subnet_file(&after);
	free_subnet_file(&sf);
	return 0;
}

static int run_missing(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf1, sf2, sf3;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	DIE_ON_BAD_FILE(argv[2]);
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0) {
		free_subnet_file(&sf1);
		BAD_FILE(argv[3]);
		return res;
	}
	res = missing_routes(&sf1, &sf2, &sf3);
	if (res < 0) {
		free_subnet_file(&sf2);
		free_subnet_file(&sf1);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf3, nof->output_fmt);
	free_subnet_file(&sf1);
	free_subnet_file(&sf2);
	free_subnet_file(&sf3);
	return 0;
}

static int run_uniq(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf1, sf2, sf3;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	DIE_ON_BAD_FILE(argv[2]);
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0) {
		free_subnet_file(&sf1);
		BAD_FILE(argv[3]);
		return res;
	}
	res = uniq_routes(&sf1, &sf2, &sf3);
	if (res < 0) {
		free_subnet_file(&sf1);
		free_subnet_file(&sf2);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf3, nof->output_fmt);
	free_subnet_file(&sf1);
	free_subnet_file(&sf2);
	free_subnet_file(&sf3);
	return 0;
}

static int run_paip(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file paip, sf;
	struct st_options *nof = st_options;

	res = load_ipam_no_EA(argv[2], &paip, nof);
	DIE_ON_BAD_FILE(argv[2]);
	res = load_netcsv_file(argv[3], &sf, nof);
	if (res < 0) {
		free_subnet_file(&paip);
		BAD_FILE(argv[3]);
		return res;
	}
	print_file_against_paip(&sf, &paip, nof);
	free_subnet_file(&sf);
	free_subnet_file(&paip);
	return 0;
}

static int run_ipam_getea(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf;
	struct ipam_file  ipam;
	struct st_options *nof = st_options;

	res = load_ipam(argv[2], &ipam, nof);
	DIE_ON_BAD_FILE(argv[2]);
	res = load_netcsv_file(argv[3], &sf, nof);
	if (res < 0) {
		free_ipam_file(&ipam);
		BAD_FILE(argv[3]);
		return res;
	}
	if (sf.nr == 0) {
		fprintf(stderr, "empty file %s\n", (argv[3] == NULL ? "<stdin>" : argv[3]));
		free_subnet_file(&sf);
		free_ipam_file(&ipam);
		return 1;
	}

	res = populate_sf_from_ipam(&sf, &ipam);
	if (res < 0) {
		free_ipam_file(&ipam);
		return 1;
	}
	if (nof->print_header)
		fprint_route_header(nof->output_file, &sf.routes[0], nof->output_fmt);
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free_subnet_file(&sf);
	free_ipam_file(&ipam);
	return 0;
}

static int run_grep(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;
	int res;

	res = network_grep_file(argv[2], nof, argv[3]);
	if (res < 0)
		return res;
	return 0;
}

static int run_filter(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	if (!strcmp(argv[2], "help") && argv[3] == NULL) {
		fprint_routefilter_help(stdout);
		return 0;
	}
	if (argv[3] == NULL) { /* read from stdin */
		res = load_netcsv_file(NULL, &sf, nof);
		if (res < 0)
			return res;
		if (nof->print_header)
			fprint_route_header(nof->output_file, &sf.routes[0], nof->output_fmt);
		res = subnet_file_filter(&sf, argv[2]);
	} else {
		res = load_netcsv_file(argv[2], &sf, nof);
		DIE_ON_BAD_FILE(argv[2]);
		if (nof->print_header)
			fprint_route_header(nof->output_file, &sf.routes[0], nof->output_fmt);
		res = subnet_file_filter(&sf, argv[3]);
	}
	if (res < 0) {
		free_subnet_file(&sf);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free_subnet_file(&sf);
	return 0;
}

static int run_bgp_filter(int argc, char **argv, void *st_options)
{
	int res;
	struct bgp_file sf;
	struct st_options *nof = st_options;

	if (!strcmp(argv[2], "help") && argv[3] == NULL) {
		fprint_bgpfilter_help(stdout);
		return 0;
	}
	if (argv[3] == NULL) { /* read from stdin */
		res = load_bgpcsv(NULL, &sf, nof);
		if (res < 0)
			return res;
		res = bgp_file_filter(&sf, argv[2]);
	} else {
		res = load_bgpcsv(argv[2], &sf, nof);
		DIE_ON_BAD_FILE(argv[2]);
		res = bgp_file_filter(&sf, argv[3]);
	}
	if (res < 0) {
		free_bgp_file(&sf);
		return res;
	}
	fprint_bgproute_fmt(nof->output_file, NULL, nof->bgp_output_fmt);
	fprint_bgp_file(nof->output_file, &sf);
	free_bgp_file(&sf);
	return 0;
}

static int run_ipam_filter(int argc, char **argv, void *st_options)
{
	int res;
	struct ipam_file sf;
	struct st_options *nof = st_options;

	if (!strcmp(argv[2], "help") && argv[3] == NULL) {
		fprint_ipamfilter_help(stdout);
		return 0;
	}
	if (argv[3] == NULL) { /* read from stdin */
		res = load_ipam(NULL, &sf, nof);
		if (res < 0)
			return res;
		fprint_ipam_header(nof->output_file, &sf.lines[0], nof->ipam_output_fmt);
		res = ipam_file_filter(&sf, argv[2]);
	} else {
		res = load_ipam(argv[2], &sf, nof);
		DIE_ON_BAD_FILE(argv[2]);
		fprint_ipam_header(nof->output_file, &sf.lines[0], nof->ipam_output_fmt);
		res = ipam_file_filter(&sf, argv[3]);
	}
	if (res < 0) {
		free_ipam_file(&sf);
		return res;
	}
	fprint_ipam_file_fmt(nof->output_file, &sf, nof->ipam_output_fmt);
	free_ipam_file(&sf);
	return 0;
}

static int run_convert(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;

	run_csvconverter(argv[2], argv[3], nof);
	return 0;
}

static int run_routesimplify1(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	nof->simplify_mode = 0;
	res = load_netcsv_file(argv[2], &sf, nof);
	DIE_ON_BAD_FILE(argv[2]);

	res = route_file_simplify(&sf, nof->simplify_mode);
	if (res < 0) {
		free_subnet_file(&sf);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free_subnet_file(&sf);
	return 0;
}

static int run_routesimplify2(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	nof->simplify_mode = 1;
	res = load_netcsv_file(argv[2], &sf, nof);
	DIE_ON_BAD_FILE(argv[2]);

	res = route_file_simplify(&sf, nof->simplify_mode);
	if (res < 0) {
		free_subnet_file(&sf);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free_subnet_file(&sf);
	return 0;
}

static int run_common(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf1, sf2, sf3;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	DIE_ON_BAD_FILE(argv[2]);
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0) {
		BAD_FILE(argv[3]);
		free_subnet_file(&sf1);
		return res;
	}
	res = subnet_file_merge_common_routes(&sf1, &sf2, &sf3);
	if (res >= 0)
		fprint_subnet_file_fmt(nof->output_file, &sf3, nof->output_fmt);
	free_subnet_file(&sf3);
	free_subnet_file(&sf2);
	free_subnet_file(&sf1);
	if (res < 0)
		return res;
	return 0;
}

static int run_addfiles(int argc, char **argv, void *st_options)
{
	int res;
	unsigned long i, j;
	struct subnet_file sf1, sf2, sf3;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	DIE_ON_BAD_FILE(argv[2]);
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0) {
		BAD_FILE(argv[3]);
		free_subnet_file(&sf1);
		return res;
	}
	res = alloc_subnet_file(&sf3, sf1.nr + sf2.nr);
	if (res < 0) {
		free_subnet_file(&sf2);
		free_subnet_file(&sf1);
		return res;
	}
	debug_timing_start(2);
	for (i = 0; i < sf1.nr; i++)
		copy_route(&sf3.routes[i], &sf1.routes[i]);
	for (j = 0; j < sf2.nr; j++)
		copy_route(&sf3.routes[i + j], &sf2.routes[j]);
	sf3.nr = i + j;
	/* since the routes comes from different files, we wont compare the GW */
	res = subnet_file_simplify(&sf3);
	if (res < 0) {
		free_subnet_file(&sf2);
		free_subnet_file(&sf1);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf3, nof->output_fmt);
	free_subnet_file(&sf3);
	free_subnet_file(&sf2);
	free_subnet_file(&sf1);
	debug_timing_end(2);
	return 0;
}

static int run_sort(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf, nof);
	DIE_ON_BAD_FILE(argv[2]);

	res = subnet_sort_by(&sf, "prefix");
	if (res < 0) {
		free_subnet_file(&sf);
		return res;
	}
	if (nof->print_header)
		fprint_route_header(nof->output_file, &sf.routes[0], nof->output_fmt);
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free_subnet_file(&sf);
	return 0;
}

static int run_sortby(int argc, char **argv, void *st_options)
{
	struct subnet_file sf;
	int res;
	struct st_options *nof = st_options;

	if (!strncmp(argv[2], "help", strlen(argv[2]))) {
		subnet_available_cmpfunc(stderr);
		return 0;
	}
	res = load_netcsv_file(argv[3], &sf, st_options);
	DIE_ON_BAD_FILE(argv[3]);

	res = subnet_sort_by(&sf, argv[2]);
	if (res == -1664) {
		fprintf(stderr, "Cannot sort by '%s'\n", argv[2]);
		fprintf(stderr, "You can sort by :\n");
		free_subnet_file(&sf);
		subnet_available_cmpfunc(stderr);
		return res;
	}
	if (nof->print_header)
		fprint_route_header(nof->output_file, &sf.routes[0], nof->output_fmt);
	fprint_subnet_file(nof->output_file, &sf, 3);
	free_subnet_file(&sf);
	return 0;
}

static int run_sum(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;
	unsigned long long sum;

	res = load_netcsv_file(argv[2], &sf, nof);
	DIE_ON_BAD_FILE(argv[2]);

	sum = sum_subnet_file(&sf);
	fprintf(nof->output_file, "Sum : %llu\n", sum);
	free_subnet_file(&sf);
	return 0;
}

static int run_subnetagg(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf, nof);
	DIE_ON_BAD_FILE(argv[2]);
	res = aggregate_route_file(&sf, 0);
	if (res < 0) {
		free_subnet_file(&sf);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free_subnet_file(&sf);
	return 0;
}

static int run_routeagg(int argc, char **argv, void *st_options)
{
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf, nof);
	DIE_ON_BAD_FILE(argv[2]);

	res = aggregate_route_file(&sf, 1);
	if (res < 0) {
		free_subnet_file(&sf);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free_subnet_file(&sf);
	return 0;
}

static int run_remove_file(int argc, char **argv, void *st_options)
{
	struct subnet_file sf1, sf2, sf3;
	struct st_options *nof = st_options;
	int res;

	res = load_netcsv_file(argv[2], &sf1, nof);
	DIE_ON_BAD_FILE(argv[2]);
	res = load_netcsv_file(argv[3], &sf3, nof);
	if (res < 0) {
		BAD_FILE(argv[3]);
		free_subnet_file(&sf1);
		return res;
	}
	subnet_file_remove_file(&sf1, &sf2, &sf3);
	if (res < 0) {
		free_subnet_file(&sf1);
		free_subnet_file(&sf2);
		free_subnet_file(&sf3);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf2, nof->output_fmt);
	free_subnet_file(&sf1);
	free_subnet_file(&sf2);
	free_subnet_file(&sf3);
	return 0;
}

static int run_remove(int argc, char **argv, void *st_options)
{
	struct subnet subnet1, subnet2, *r;
	struct subnet_file sf1, sf2;
	struct st_options *nof = st_options;
	int n, i, res;

	if (!strcasecmp(argv[2], "subnet")) {
		res = get_subnet_or_ip(argv[3], &subnet1);
		if (res < 0) {
			printf("Invalid IP %s\n", argv[3]);
			return -1;
		}
		res = get_subnet_or_ip(argv[4], &subnet2);
		if (res < 0) {
			printf("Invalid IP %s\n", argv[4]);
			return -1;
		}
		r = subnet_remove(&subnet1, &subnet2, &n);
		if (n == -1)
			return -1;

		for (i = 0; i < n; i++)
			st_printf("%P\n", r[i]);
		st_free(r, n * sizeof(struct subnet));
		return 0;
	} else  if (!strcasecmp(argv[2], "file")) {
		res = load_netcsv_file(argv[3], &sf1, nof);
		DIE_ON_BAD_FILE(argv[3]);

		res = get_subnet_or_ip(argv[4], &subnet2);
		if (res < 0) {
			free_subnet_file(&sf1);
			printf("Invalid IP %s\n", argv[4]);
			return -1;
		}
		subnet_file_remove_subnet(&sf1, &sf2, &subnet2);
		fprint_subnet_file_fmt(nof->output_file, &sf2, nof->output_fmt);
		free_subnet_file(&sf1);
		free_subnet_file(&sf2);
		return 0;
	}
	fprintf(stderr, "Invalid objet %s after %s, expecting 'subnet' or 'file'\n",
			argv[2], argv[1]);
	return -1;
}

static int run_split(int argc, char **argv, void *st_options)
{
	struct subnet subnet;
	struct st_options *nof = st_options;
	int res;

	res = get_subnet_or_ip(argv[2], &subnet);
	if (res != IPV6_N && res != IPV4_N) {
		fprintf(stderr, "split works on subnet and '%s' is not\n", argv[2]);
		return -1;
	}
	res = subnet_split(nof->output_file, &subnet, argv[3]);
	return res;
}

static int run_split_2(int argc, char **argv, void *st_options)
{
	struct subnet subnet;
	struct st_options *nof = st_options;
	int res;

	res = get_subnet_or_ip(argv[2], &subnet);
	if (res != IPV6_N && res != IPV4_N) {
		fprintf(stderr, "split works on subnet and '%s' is not\n", argv[2]);
		return -1;
	}
	res = subnet_split_2(nof->output_file, &subnet, argv[3]);
	return res;
}

static int run_scanf(int argc, char **argv, void *st_options)
{
	int res;
	struct sto o[SCANF_MAX_OBJECTS];
	int num_cs = count_cs(argv[3]);
	struct st_options *nof = st_options;

	if (num_cs > SCANF_MAX_OBJECTS) {
		fprintf(stderr, "You want to collect %d objects, but max is %d\n",
				num_cs, SCANF_MAX_OBJECTS);
		return -1;
	}
	res = sto_sscanf(argv[2], argv[3], o, SCANF_MAX_OBJECTS);
	if (res < 0) {
		fprintf(stderr, "no match\n");
		return res;
	}
	/* user-defined output format or default one */
	sto_printf(nof->scanf_output_fmt, o, res);
	return 0;
}

/* argv[2] == file, argv[3] == expr */
static int run_fscanf(int argc, char **argv, void *st_options)
{
	int res;
	struct sto o[SCANF_MAX_OBJECTS];
	FILE *f;
	char buffer[FSCANF_LINE_LENGTH];
	char *s;
	unsigned long line = 0;
	struct st_options *nof = st_options;
	int num_cs = count_cs(argv[3]);

	if (num_cs > SCANF_MAX_OBJECTS) {
		fprintf(stderr, "You want to collect %d objects, but max is %d\n",
				num_cs, SCANF_MAX_OBJECTS);
		return -1;
	}
	f = fopen(argv[2], "r");
	if (f == NULL) {
		fprintf(stderr, "Cannot open %s for reading\n", argv[2]);
		return -1;
	}
	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		res = sto_sscanf(s, argv[3], o, SCANF_MAX_OBJECTS);
		/* we print the result only on exact match but not partial match */
		if (res >= num_cs)
			sto_printf(nof->scanf_output_fmt, o, res);
	}
	return 0;
}

static int run_help(int argc, char **argv, void *st_options)
{
	struct st_options *o = st_options;

	usage(argc, argv, o);
	return 0;
}

static int run_version(int argc, char **argv, void *st_options)
{
	printf("%s version %s by Mahmoud Basset\n", PROG_NAME, PROG_VERS);
#ifdef __DATE__
	printf("Compiled %s\n", __DATE__);
#endif
	printf("%s is licenced under the GPLv2.0\n", PROG_NAME);
	return 0;
}

static int run_confdesc(int argc, char **argv, void *st_options)
{
	printf("-%s use 'st.conf' as the default configuration file\n", PROG_NAME);
	printf("-It is used to describe your CSVs or your IPAM format\n");
	printf("-format of a line file is :\n");
	printf("FIELD=VALUE\n");
	printf("-comments starts with a '#'\n\n");
	printf("-Fields you can configure :\n");

	config_file_describe();
	return 0;
}

static int run_bgpcmp(int argc, char **argv, void *st_options)
{
	struct bgp_file sf1;
	struct bgp_file sf2;
	int res;

	res = load_bgpcsv(argv[2], &sf1, st_options);
	DIE_ON_BAD_FILE(argv[2]);
	res = load_bgpcsv(argv[3], &sf2, st_options);
	if (res < 0) {
		BAD_FILE(argv[3]);
		free_bgp_file(&sf1);
		return res;
	}

	compare_bgp_file(&sf1, &sf2, st_options);
	free_bgp_file(&sf1);
	free_bgp_file(&sf2);
	return 0;
}

static int run_bgpsortby(int argc, char **argv, void *st_options)
{
	struct bgp_file sf;
	int res;
	struct st_options *o = st_options;

	if (!strncmp(argv[2], "help", strlen(argv[2]))) {
		bgp_available_cmpfunc(stderr);
		return 0;
	}
	res = load_bgpcsv(argv[3], &sf, st_options);
	DIE_ON_BAD_FILE(argv[3]);

	res = bgp_sort_by(&sf, argv[2]);
	if (res == -1664) {
		fprintf(stderr, "Cannot sort by '%s'\n", argv[2]);
		fprintf(stderr, "You can sort by :\n");
		bgp_available_cmpfunc(stderr);
		free_bgp_file(&sf);
		return res;
	}
	fprint_bgp_file_header(o->output_file);
	fprint_bgp_file(o->output_file, &sf);
	free_bgp_file(&sf);
	return 0;
}

static int run_test(int argc, char **argv, void *st_options)
{
	struct ipam_file sf1;
	struct st_options *o = st_options;
	int res;

	res = load_ipam(argv[2], &sf1, st_options);
	if (res < 0)
		return res;
	fprint_ipam_file_fmt(stdout, &sf1, o->ipam_output_fmt);
	return 0;
}

static int run_gen_expr(int argc, char **argv, void *st_options)
{
	struct generic_expr e;
	int res;

	init_generic_expr(&e, argv[2], int_compare);
	res = run_generic_expr(argv[2], strlen(argv[2]), &e);
	printf("res=%d\n", res);
	return 0;
}

static int run_test2(int argc, char **argv, void *st_options)
{
	int res;
	struct sto o[40];

	res = sto_sscanf(argv[2], argv[3], o, 40);
	printf("%d\n", res);
	sto_printf("%O0 %O1 %O2 %O3 %O4 %O5 %O6 %O7\n", o, res);
	return 0;
}
/*
 * OPTION HANDLERS
 */
static int option_verbose(int argc, char **argv, void *st_options)
{
	debugs_level[__D_ALL] = 1;
	return 0;
}

static int option_verbose2(int argc, char **argv, void *st_options)
{
	debugs_level[__D_ALL] = 2;
	return 0;
}

static int option_delim(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;
	int i;

	debug(PARSEOPTS, 3, "changing delim to : '%s'\n", argv[1]);
	i = strxcpy(nof->delim, argv[1], sizeof(nof->delim));
	if (i >= sizeof(nof->delim))
		fprintf(stderr, "too many delimiters, MAX %d\n", MAX_DELIM);
	return 0;
}

static int option_grepfield(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;
	int a, res;

	debug(PARSEOPTS, 3, "grepping only field %s\n", argv[1]);
	if (!isUnsignedInt(argv[1])) {
		fprintf(stderr, "invalid grep_field %s\n", argv[1]);
		return -1;
	}
	a = string2int(argv[1], &res);
	if (res < 0)
		return res;
	nof->grep_field = a;
	return 0;
}

static int option_output(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;

	debug(PARSEOPTS, 3, "changing ouput file to : \"%s\"\n", argv[1]);
	nof->output_file = fopen(argv[1], "w");
	if (nof->output_file == NULL) {
		fprintf(stderr, "cannot open %s for writing, using standard output\n", argv[1]);
		nof->output_file = stdout;
	}
	return 0;
}

static int option_debug(int argc, char **argv, void *st_options)
{
	debug(PARSEOPTS, 3, "debug st_options : \"%s\"\n", argv[1]);
	if (!strcmp(argv[1], "help") || !strcmp(argv[1], "list")) {
		debug_usage();
		return -1;
	}
	parse_debug(argv[1]);
	return 0;
}

static int option_config(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;

	debug(PARSEOPTS, 3, "Using configuration file : \"%s\"\n", argv[1]);
	nof->config_file = argv[1];
	return 0;
}

static int option_addr_compress(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;
	int a, res;

	if (!isUnsignedInt(argv[1])) {
		fprintf(stderr, "expected an unsigned int after option '-p', but got '%s'\n",
				argv[1]);
		return 0;
	}
	a = string2int(argv[1], &res);
	if (res < 0)
		return -1;
	if (a < 0 || a > 3) {
		fprintf(stderr, "out of bound value for option '-p', [0-3], got '%s'\n", argv[1]);
		return 0;
	}
	nof->ip_compress_mode = a;
	return 0;
}

static int option_fmt(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;
	int res;

	res = strxcpy(nof->output_fmt, argv[1], sizeof(nof->output_fmt));
	if (res >= sizeof(nof->output_fmt)) {
		fprintf(stderr, "cannot change FMT, '%s' is too long, max '%d' char\n",
			argv[1], (int)sizeof(nof->output_fmt));
		return -1;
	}
	/* sizeof(xxx_output_fmt) are the same, no need to check return value */
	res = strxcpy(nof->bgp_output_fmt, argv[1], sizeof(nof->bgp_output_fmt));
	res = strxcpy(nof->ipam_output_fmt, argv[1], sizeof(nof->ipam_output_fmt));
	res = strxcpy(nof->scanf_output_fmt, argv[1], sizeof(nof->scanf_output_fmt));

	debug(PARSEOPTS, 3, "Changing default FMT : '%s'\n", argv[1]);
	return 0;
}

static int option_rt(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;

	nof->rt = 1;
	debug(PARSEOPTS, 3, "Convert will now print route type as a comment\n");
	return 0;
}

static int option_ecmp(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;

	nof->ecmp = 1;
	debug(PARSEOPTS, 3, "Convert will now print 2 routes in case of ECMP\n");
	return 0;
}

static int option_ipam_ea(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;
	int res;

	res = strxcpy(nof->ipam_ea, argv[1], sizeof(nof->ipam_ea));
	if (res >= sizeof(nof->ipam_ea)) {
		fprintf(stderr, "too many EA, size must be < %d\n", (int)sizeof(nof->ipam_ea));
		return -1;
	}
	debug(PARSEOPTS, 3, "The following Ipam EA will be collected: '%s'\n", argv[1]);
	return 0;
}

static int option_noheader(int argc, char **argv, void *st_options)
{
	struct st_options *nof = st_options;

	nof->print_header = 0;
	debug(PARSEOPTS, 3, "Netcsv header wont be printed\n");
	return 0;
}

/* ensure a core dump is generated in case of BUG
 * subnettool is bug free of course :)
 * man page says it is POSIX, let s hope so
 */
static void allow_core_dumps(void)
{
	struct rlimit limits;
	int res;

	limits.rlim_cur = RLIM_INFINITY;
	limits.rlim_max = RLIM_INFINITY;
	res = setrlimit(RLIMIT_CORE, &limits);
	if (res == -1)
		fprintf(stderr, "couldnt set CORE dump size\n");
}

int main(int argc, char **argv)
{
	struct st_options nof;
	int res, res2;
	char *s;
	char conf_abs_path[256];

	memset(&nof, 0, sizeof(nof));
	nof.output_file      = stdout;
	/* full IPv6 address compression  with IPv4 mapped/compatible support*/
	nof.ip_compress_mode = 3;
	nof.print_header     = 1;
	strcpy(nof.ipam_ea, "comment");
	allow_core_dumps();
	res = generic_parse_options(argc, argv, PROG_NAME, &nof);
	if (res < 0)
		exit(1);
	argc = argc - res;
	argv = argv + res;
	if (argc < 2)
		exit(1);

	if (nof.config_file == NULL) {
		/* default config file is $HOME/st.conf */
		s = getenv("HOME");
		if (s == NULL)
			s = ".";
		res = strxcpy(conf_abs_path, s, sizeof(conf_abs_path));
		if (res >= sizeof(conf_abs_path)) {
			fprintf(stderr, "buffer overrun attempt, $HOME too big\n");
			exit(2);
		}
		res2 = strxcpy(conf_abs_path + res, "/st.conf",
				sizeof(conf_abs_path) - res);
		if (res2 >= sizeof(conf_abs_path) - res) {
			fprintf(stderr, "buffer overrun attempt, $HOME/st.conf too big\n");
			exit(2);
		}

		nof.config_file = conf_abs_path;
		res = open_config_file(nof.config_file, &nof);
		if (res == -1) { /* try to open ./st.conf */
			strcpy(nof.config_file, "./st.conf");
			open_config_file(nof.config_file, &nof);
		}
	} else
		open_config_file(nof.config_file, &nof);

	/* if delims are not set, set the default one*/
	if (strlen(nof.delim) == 0)
		strcpy(nof.delim, ";");
	if (strlen(nof.ipam_delim) == 0)
		strcpy(nof.ipam_delim, ",");
	/* if the default output format has not been set */
	if (strlen(nof.output_fmt) < 2)
		strcpy(nof.output_fmt, default_fmt);
	if (strlen(nof.bgp_output_fmt) < 2)
		strcpy(nof.bgp_output_fmt, bgp_default_fmt);
	if (strlen(nof.scanf_output_fmt) < 2)
		strcpy(nof.scanf_output_fmt, scanf_default_fmt);

	res = generic_command_run(argc, argv, PROG_NAME, &nof);
	fclose(nof.output_file);
	exit(res);
}
