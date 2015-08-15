/*
 * subnet tools MAIN
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
#include <ctype.h>
#include "debug.h"
#include <sys/types.h>
#include <sys/resource.h>
#include "iptools.h"
#include "routetocsv.h"
#include "utils.h"
#include "generic_csv.h"
#include "subnet_tool.h"
#include "generic_command.h"
#include "config_file.h"
#include "st_printf.h"
#include "st_scanf.h"
#include "ipinfo.h"
#include "bgp_tool.h"
#include "generic_expr.h"
#include "prog-main.h"

const char *default_fmt     = "%I;%m;%D;%G;%C";
const char *bgp_default_fmt = "%v;%5T;%4B;%16P;%16G;%10M;%10L;%10w;%6o;%A";

struct file_options fileoptions[] = {
	{ FILEOPT_LINE(ipam_prefix_field, struct st_options, TYPE_STRING), "IPAM CSV header field describing the prefix"  },
	{ FILEOPT_LINE(ipam_mask, struct st_options, TYPE_STRING), "IPAM CSV header field describing the mask" },
	{ FILEOPT_LINE(ipam_comment1, struct st_options, TYPE_STRING), "IPAM CSV header field describing comment" },
	{ FILEOPT_LINE(ipam_comment2, struct st_options, TYPE_STRING),  "IPAM CSV header field describing comment" },
	{ FILEOPT_LINE(ipam_delim, struct st_options, TYPE_STRING),  "IPAM CSV delimitor" },
	{ "netcsv_delim", TYPE_STRING, sizeofmember(struct st_options, delim), offsetof(struct st_options, delim), "CSV delimitor" },
	{ FILEOPT_LINE(netcsv_prefix_field, struct st_options, TYPE_STRING), "Subnet CSV header field describing the prefix" },
	{ FILEOPT_LINE(netcsv_mask, struct st_options, TYPE_STRING), "Subnet CSV header field describing the mask" },
	{ FILEOPT_LINE(netcsv_comment, struct st_options, TYPE_STRING), "Subnet CSV header field describing the comment" },
	{ FILEOPT_LINE(netcsv_device, struct st_options, TYPE_STRING), "Subnet CSV header field describing the device" },
	{ FILEOPT_LINE(netcsv_gw, struct st_options, TYPE_STRING), "Subnet CSV header field describing the gateway" },
	{ FILEOPT_LINE(output_fmt, struct st_options, TYPE_STRING), "Default Output Format String" },
	{ FILEOPT_LINE(subnet_off, struct st_options, TYPE_INT) },
	{NULL,                  0, 0}
};


static int run_diff(int argc, char **argv, void *st_options);
static int run_compare(int argc, char **argv, void *st_options);
static int run_missing(int argc, char **argv, void *st_options);
static int run_paip(int argc, char **argv, void *st_options);
static int run_grep(int argc, char **argv, void *st_options);
static int run_convert(int argc, char **argv, void *st_options);
static int run_simplify1(int argc, char **argv, void *st_options);
static int run_simplify2(int argc, char **argv, void *st_options);
static int run_common(int argc, char **argv, void *st_options);
static int run_addfiles(int argc, char **argv, void *st_options);
static int run_sort(int argc, char **argv, void *st_options);
static int run_sortby(int argc, char **argv, void *st_options);
static int run_filter(int argc, char **argv, void *st_options);
static int run_bgp_filter(int argc, char **argv, void *st_options);
static int run_sum(int argc, char **argv, void *st_options);
static int run_scanf(int argc, char **argv, void *st_options);
static int run_subnetagg(int argc, char **argv, void *st_options);
static int run_routeagg(int argc, char **argv, void *st_options);
static int run_remove(int argc, char **argv, void *st_options);
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
static int run_test(int argc, char **argv, void *st_options);
static int run_gen_expr(int argc, char **argv, void *st_options);
static int run_test2(int argc, char **argv, void *st_options);

static int option_verbose(int argc, char **argv, void *st_options);
static int option_delim(int argc, char **argv, void *st_options);
static int option_grepfield(int argc, char **argv, void *st_options);
static int option_output(int argc, char **argv, void *st_options);
static int option_debug(int argc, char **argv, void *st_options);
static int option_config(int argc, char **argv, void *st_options);
static int option_addr_compress(int argc, char **argv, void *st_options);
static int option_fmt(int argc, char **argv, void *st_options);
static int option_rt(int argc, char **argv, void *st_options);
static int option_ecmp(int argc, char **argv, void *st_options);

struct st_command commands[] = {
	{ "echo",	&run_echo,	2},
	{ "print",	&run_print,	0},
	{ "bgpprint",	&run_bgpprint,	0},
	{ "relation",	&run_relation,	2},
	{ "bgpcmp",	&run_bgpcmp,	2},
	{ "bgpsortby",	&run_bgpsortby,	1},
	{ "ipinfo",	&run_ipinfo,	1},
	{ "diff",	&run_diff,	2},
	{ "compare",	&run_compare,	2},
	{ "missing",	&run_missing,	2},
	{ "paip",	&run_paip,	1},
	{ "ipam",	&run_paip,	1},
	{ "grep",	&run_grep,	2},
	{ "convert",	&run_convert,	1},
	{ "simplify1",	&run_simplify1,	1},
	{ "simplify2",	&run_simplify2,	1},
	{ "common",	&run_common,	2},
	{ "addfiles",	&run_addfiles,	2},
	{ "sort",	&run_sort,	0},
	{ "sortby",	&run_sortby,	1},
	{ "filter",	&run_filter,	1},
	{ "bgpfilter",	&run_bgp_filter,1},
	{ "sum",	&run_sum,	1},
	{ "subnetagg",	&run_subnetagg,	1},
	{ "routeagg",	&run_routeagg,	1},
	{ "removesubnet", &run_remove,	3},
	{ "split",	&run_split,	2},
	{ "split2",	&run_split_2,	2},
	{ "scanf",	&run_scanf,	2},
	{ "help",	&run_help,	0},
	{ "version",	&run_version,	0},
	{ "confdesc",	&run_confdesc,	0},
	{ "exprtest",	&run_gen_expr,	1, 1},
	{ "test",	&run_test,	1, 1},
	{ "test2",	&run_test2,	2, 1},
	{NULL, 		NULL,		0}
};

struct st_command options[] = {
	{"-V",		&option_verbose,	0},
	{"-d",		&option_delim,		1},
	{"-grep_field",	&option_grepfield,	1},
	{"-o",		&option_output,		1},
	{"-D",		&option_debug,		1},
	{"-c",		&option_config,		1},
	{"-p",		&option_addr_compress,	1},
	{"-fmt",	&option_fmt,		1},
	{"-rt",		&option_rt,		0},
	{"-ecmp",	&option_ecmp,		0},
	{NULL, NULL, 0}
};

void usage() {
	printf("Usage: %s [OPTIONS] COMMAND ARGUMENTS ....\n", PROG_NAME);
	printf("\n");
	printf("\nCOMMAND := \n");
	printf("Subnet arithmetic\n");
	printf("-----------------\n");
	printf("relation IP1 IP2    : prints a relationship between IP1 and IP2\n");
	printf("split S, <l1,l2,..> : split subnet S l1 times, the result l2 times, and so on..\n");
	printf("split2 S, <m1,m2,..>: split subnet S with mask m1, then m2, and so on...\n");
	printf("removesub TYPE O1 S1: remove Subnet S from Object O1; if TYPE=file O1=ile, if TYPE=subnet 01=subnet\n");
	printf("ipinfo IP|all|IPvX  : prints information about IP, or all known subnets (all, IPv4 or IPv6)\n");
	printf("\n");
	printf("Route file simplification\n");
	printf("-------------------------\n");
	printf("sort FILE1          : sort CSV FILE1\n");
	printf("sortby name file    : sort CSV file by (prefix|gw|mask), prefix is always a tie-breaker\n");
	printf("sortby help	    : print available sort options\n");
	printf("subnetagg FILE1     : sort and aggregate subnets in CSV FILE1; GW is not checked\n");
	printf("routeagg  FILE1     : sort and aggregate subnets in CSV FILE1; GW is checked\n");
	printf("simplify1 FILE1     : simplify CSV subnet file FILE1; duplicate or included networks are removed; GW is checked\n");
	printf("simplify2 FILE1     : simplify CSV subnet file FILE1; prints redundant routes that can be removed\n");
	printf("\n");
	printf("Route file comparison\n");
	printf("---------------------\n");
	printf("compare FILE1 FILE2 : compare FILE1 & FILE2, printing subnets in FILE1 INCLUDED in FILE2\n");
	printf("missing FILE1 FILE2 : prints subnets from FILE1 that are not covered by FILE2; GW is not checked\n");
	printf("ipam <IPAM> FILE1   : load IPAM, and print FILE1 subnets with comment extracted from IPAM\n");
	printf("diff FILE1 FILE2    : diff FILE1 & FILE2 (EXPERIMENTAL)\n");
	printf("common FILE1 FILE2  : merge CSV subnet files FILE1 & FILE2; prints common routes only; GW isn't checked\n");
	printf("addfiles FILE1 FILE2: merge CSV subnet files FILE1 & FILE2; prints the sum of both files\n");
	printf("grep FILE prefix    : grep FILE for prefix/mask\n");
	printf("filter FILE EXPR    : grep netcsv   FILE using regexp EXPR\n");
	printf("filter help         : prints help about bgp filters\n");
	printf("bgpfilter FILE EXPR : grep bgp_file FILE using regexp EXPR\n");
	printf("bgpfilter help      : prints help about bgp filters\n");
	printf("\n");
	printf("Miscellaneous route file tools\n");
	printf("------------------------------\n");
	printf("print FILE1         : just read & print FILE1; use a -fmt FMT to print CSV fields you want\n");
	printf("bgpprint FILE1      : just read & print BGP FILE1; use a -fmt FMT to print CSV fields you want\n");
	printf("sum IPv4FILE        : get total number of hosts included in the list of subnets\n");
	printf("sum IPv6FILE        : get total number of /64 subnets included\n");
	printf("\n");
	printf("BGP route file tools\n");
	printf("--------------------\n");
	printf("bgpcmp file1 file2  : show what changed in BGP file file1 & file2\n");
	printf("bgpsortby name file : sort BGP file by (prefix|gw|MED|LOCALPREF), prefix is always a tie-breaker\n");
	printf("bgpsortby help	    : print available sort options\n");
	printf("\n");
	printf("IP route to CSV converters\n");
	printf("--------------------------\n");
	printf("convert PARSER FILE1: convert FILE1 to csv using parser PARSER\n");
	printf("convert help        : use '%s convert help' for available parsers \n", PROG_NAME);
	printf("\n");
	printf("DEBUG and help\n");
	printf("--------------\n");
	printf("echo FMT ARG2       : try to get subnet from ARG2 and echo it according to FMT\n");
	printf("scanf STRING1 FMT   : scan STRING1 according to scanf-like format FMT\n");
	printf("confdesc            : print a small explanation of %s configuration file\n", PROG_NAME);
	printf("help                : This HELP \n");
	printf("version             : %s version \n", PROG_NAME);
	printf("\nOPTIONS := \n");
	printf("-d <delim>      : change the default field delim (;) \n");
	printf("-c <file >      : use config file <file>  instead of st.conf\n");
	printf("-o <file >      : write output in <file> \n");
	printf("-rt		: when converting routing table, set route type as comment\n");
	printf("-ecmp		: when converting routing table, print all routes in case of ECMP\n");
	printf("-grep_field N   : grep field N only\n");
	printf("-D <debug>      : DEBUG MODE ; use '%s -D help' for more info\n", PROG_NAME);
	printf("-fmt            : change the output format (default :%s)\n", default_fmt);
	printf("-V              : verbose mode; same as '-D all:1'\n\n");
	printf("INPUT CSV format :=\n");
	printf("- Input subnet/routes files SHOULD have a CSV header describing its structure (prefix, mask,, GW, comment, etc...)\n");
	printf("- Input subnet/routes files without a CSV header are assumed to be : prefix;mask;GW;comment or prefix;mask;comment\n");
	printf("- default CSV header is 'prefix;mask;device;GW;comment'\n");
	printf("- CSV header can be changed by using the configuration file (subnettools confdesc for more info)\n");
	printf("- Input IPAM CSV MUST have a CSV header; there is a defaut header, but it is derived from my company's one\n");
	printf("- So IPAM CSV header MUST be described in the configuration file\n");
}

void debug_usage() {
	printf("Usage: %s -D Debugs [OPTIONS] COMMAND FILES \n", PROG_NAME);
	printf("Debugs looks like : symbol1:level,symbol2:level;...\n");
	printf("symbol is the  symbol to  debug, increasing level gives more info, ie :\n");
	printf("level 1 : get more infos on errors (badly formatted file, bad IP etc...)\n");
	printf("level 4+ : print  internal program debug info ; wont tell you anything without source code \n");
	printf("level 9 : if implemented, print info for each loop of the program \n");
	printf("Example 1 :\n");
	printf("'%s -D memory:1,parseopts:2,trucmuche:3 simplify1 MYFILE'\n", PROG_NAME);
	printf("Example 2 :\n");
	printf("'%s -D loadcsv:3 sort MYFILE' will tell you every invalid line in your file.\n\n", PROG_NAME);
	printf("Available symbols :\n");
	list_debugs();
}
/*
 * COMMAND HANDLERS
 */
static int run_relation(int arc, char **argv, void *st_options) {
	int res;
	struct subnet subnet1, subnet2;

	res = get_subnet_or_ip(argv[2], &subnet1);
	if (res < 0) {
		printf("%s is not an IP\n", argv[2]);
		return 0;
	}
	res = get_subnet_or_ip(argv[3], &subnet2);
	if (res < 0) {
		printf("%s is not an IP\n", argv[3]);
		return 0;
	}
	if (subnet1.ip_ver != subnet2.ip_ver) {
		printf("IP version is different\n");
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

static int run_echo(int arc, char **argv, void *st_options) {
	int res;
	struct subnet subnet;

	res = get_subnet_or_ip(argv[3], &subnet);
	if (strstr(argv[2], "%s")) {
		fprintf(stderr, "Bad FMT, %%s is not allowed in this context\n");
		return 0;
	}
	if (res == IPV4_A || res == IPV6_A)
		st_printf(argv[2], subnet, subnet);
	else if (res == IPV4_N || res == IPV6_N)
		st_printf(argv[2], subnet, subnet, subnet);
	else
		printf("Invalid IP");
	printf("\n");
	return 0;
}

static int run_ipinfo(int arc, char **argv, void *st_options) {
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

static int run_print(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf, nof);
	if (res < 0)
		return res;
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	return 0;
}

static int run_bgpprint(int arc, char **argv, void *st_options) {
	int res;
	struct bgp_file sf;
	struct st_options *nof = st_options;

	res = load_bgpcsv(argv[2], &sf, nof);
	if (res < 0)
		return res;
	fprint_bgproute_fmt(nof->output_file, NULL, nof->bgp_output_fmt);
	fprint_bgp_file_fmt(nof->output_file, &sf, nof->bgp_output_fmt);
	return 0;
}

static int run_diff(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf1, sf2;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	if (res < 0)
		fprintf(stderr, "invalid file %s; not a CSV?\n", argv[2]);
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0)
		fprintf(stderr, "invalid file %s; not a CSV?\n", argv[3]);
	diff_files(&sf1, &sf2, nof);
	return 0;
}

static int run_compare(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf1, sf2;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	if (res < 0)
		fprintf(stderr, "invalid file %s; not a CSV?\n", argv[2]);
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0)
		fprintf(stderr, "invalid file %s; not a CSV?\n", argv[3]);
	compare_files(&sf1, &sf2, nof);
	return 0;
}

static int run_missing(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf1, sf2, sf3;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	if (res < 0) {
		fprintf(stderr, "invalid file %s; not a CSV?\n", argv[2]);
		return res;
	}
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0) {
		fprintf(stderr, "invalid file %s; not a CSV?\n", argv[3]);
		return res;
	}
	res = missing_routes(&sf1, &sf2, &sf3);
	if (res < 0)
		return res;
	fprint_subnet_file_fmt(nof->output_file, &sf3, nof->output_fmt);
	free(sf3.routes);
	free(sf2.routes);
	free(sf1.routes);
	return 0;
}

static int run_paip(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file paip, sf;
	struct st_options *nof = st_options;

	res = load_PAIP(argv[2], &paip, nof);
	if (res < 0) {
		fprintf(stderr, "invalid IPAM file %s; not a CSV?\n", argv[2]);
		return res;
	}
	res = load_netcsv_file(argv[3], &sf, nof);
	if (res < 0) {
		fprintf(stderr, "invalid file %s; not a CSV?\n", argv[3]);
		return res;
	}
	print_file_against_paip(&sf, &paip, nof);
	return 0;
}

static int run_grep(int arc, char **argv, void *st_options) {
	struct st_options *nof = st_options;
	int res;
	res = network_grep_file(argv[2], nof, argv[3]);
	if (res < 0)
		return res;
	return 0;
}

static int run_filter(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	if (argv[3] == NULL) { /* read from stdin */
		res = load_netcsv_file(NULL, &sf, nof);
		if (res < 0)
			return res;
		res = subnet_filter(&sf, argv[2]);
	} else {
		res = load_netcsv_file(argv[2], &sf, nof);
		if (res < 0)
			return res;
		res = subnet_filter(&sf, argv[3]);
	}
	if (res < 0) {
		free(sf.routes);
		fprintf(stderr, "Couldnt filter file %s\n", argv[3] ? argv[2] : "<tsdin>");
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free(sf.routes);
	return 0;
}

static int run_bgp_filter(int arc, char **argv, void *st_options) {
	int res;
	struct bgp_file sf;
	struct st_options *nof = st_options;

	if (!strcmp(argv[2], "help")) {
		fprint_bgpfilter_help(stdout);
		return 0;
	}
	if (argv[3] == NULL) { /* read from stdin */
		res = load_bgpcsv(NULL, &sf, nof);
		if (res < 0)
			return res;
		res = bgp_filter(&sf, argv[2]);
	} else {
		res = load_bgpcsv(argv[2], &sf, nof);
		if (res < 0)
			return res;
		res = bgp_filter(&sf, argv[3]);
	}
	if (res < 0) {
		free(sf.routes);
		fprintf(stderr, "Couldnt filter file %s\n", argv[3] ? argv[2] : "<tsdin>");
		return res;
	}
	fprint_bgp_file(nof->output_file, &sf);
	free(sf.routes);
	return 0;
}

static int run_convert(int arc, char **argv, void *st_options) {
	struct st_options *nof = st_options;

	run_csvconverter(argv[2], argv[3], nof);
	return 0;
}

static int run_simplify1(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	nof->simplify_mode = 0;
	res = load_netcsv_file(argv[2], &sf, nof);
	if (res < 0)
		return res;
	res = route_file_simplify(&sf, nof->simplify_mode);
	if (res < 0) {
		free(sf.routes);
		fprintf(stderr, "Couldnt simplify file %s\n", argv[2]);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free(sf.routes);
	return 0;
}

static int run_simplify2(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	nof->simplify_mode = 1;
	res = load_netcsv_file(argv[2], &sf, nof);
	if (res < 0)
		return res;
	res = route_file_simplify(&sf, nof->simplify_mode);
	if (res < 0) {
		free(sf.routes);
		fprintf(stderr, "Couldnt simplify file %s\n", argv[2]);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free(sf.routes);
	return 0;
}

static int run_common(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf1, sf2, sf3;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	if (res < 0)
		return res;
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0)
		return res;
	res = alloc_subnet_file(&sf3, sf1.nr + sf2.nr);
	if (res < 0)
		return res;
	res = subnet_file_merge_common_routes(&sf1, &sf2, &sf3);
	if (res >= 0)
		fprint_subnet_file_fmt(nof->output_file, &sf3, nof->output_fmt);
	free(sf3.routes);
	free(sf2.routes);
	free(sf1.routes);
	if (res < 0) {
		fprintf(stderr, "Couldnt merge file %s\n", argv[2]);
		return res;
	}
	return 0;
}

static int run_addfiles(int arc, char **argv, void *st_options) {
	int res;
	unsigned long i, j;
	struct subnet_file sf1, sf2, sf3;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	if (res < 0)
		return res;
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0)
		return res;

	res = alloc_subnet_file(&sf3, sf1.nr + sf2.nr);
	if (res < 0)
		return res;
	debug_timing_start(2);
	for (i = 0; i < sf1.nr; i++)
		copy_route(&sf3.routes[i], &sf1.routes[i]);
	for (j = 0; j < sf2.nr; j++)
		copy_route(&sf3.routes[i+j], &sf2.routes[j]);
	sf3.nr = i + j;
	/* since the routes comes from different files, we wont compare the GW */
	subnet_file_simplify(&sf3);
	fprint_subnet_file_fmt(nof->output_file, &sf3, nof->output_fmt);
	free(sf1.routes);
	free(sf2.routes);
	free(sf3.routes);
	debug_timing_end(2);
	return 0;
}

static int run_sort(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf, nof);

	if (res < 0)
		return res;
	res = subnet_sort_by(&sf, "prefix");
	if (res < 0) {
		fprintf(stderr, "Couldnt sort file %s\n", argv[2]);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free(sf.routes);
	return 0;
}

static int run_sortby(int arc, char **argv, void *st_options) {
	struct subnet_file sf1;
	int res;
	struct st_options *o = st_options;

	if (!strncmp(argv[2], "help", strlen(argv[2]))) {
		subnet_available_cmpfunc(stderr);
		return 0;
	}
	res = load_netcsv_file(argv[3], &sf1, st_options);
	if (res < 0)
		return res;
	res = subnet_sort_by(&sf1, argv[2]);
	if (res == -1664) {
		fprintf(stderr, "Cannot sort by '%s'\n", argv[2]);
		fprintf(stderr, "You can sort by :\n");
		subnet_available_cmpfunc(stderr);
		return res;
	}
	fprint_subnet_file(o->output_file, &sf1, 3);
	return 0;
}

static int run_sum(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;
	unsigned long long sum;

	res = load_netcsv_file(argv[2], &sf, nof);
	if (res < 0)
		return res;
	sum = sum_subnet_file(&sf);
	fprintf(nof->output_file, "Sum : %llu\n", sum);
	free(sf.routes);
	return 0;
}

static int run_subnetagg(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf, nof);
	if (res < 0)
		return res;
	res = aggregate_route_file(&sf, 0);
	if (res < 0) {
		fprintf(stderr, "Couldnt aggregate file %s\n", argv[2]);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free(sf.routes);
	return 0;
}

static int run_routeagg(int arc, char **argv, void *st_options) {
	int res;
	struct subnet_file sf;
	struct st_options *nof = st_options;

	res = load_netcsv_file(argv[2], &sf, nof);
	if (res < 0)
		return res;
	res = aggregate_route_file(&sf, 1);
	if (res < 0) {
		fprintf(stderr, "Couldnt aggregate file %s\n", argv[2]);
		return res;
	}
	fprint_subnet_file_fmt(nof->output_file, &sf, nof->output_fmt);
	free(sf.routes);
	return 0;
}

static int run_remove(int arc, char **argv, void *st_options) {
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
		if (n == -1) {
			printf("no memory for subnet_remove\n");
			return -1;
		}
		for (i = 0; i < n; i++)
			st_printf("%P\n", r[i]);
		free(r);
		return 0;
	} else  if (!strcasecmp(argv[2], "file")) {
		res = load_netcsv_file(argv[3], &sf1, nof);
		if (res < 0) {
			fprintf(stderr, "Invalid csv file %s\n", argv[3]);
			return res;
		}
		res = get_subnet_or_ip(argv[4], &subnet2);
		if (res < 0) {
			printf("Invalid IP %s\n", argv[4]);
			return -1;
		}
		subnet_file_remove(&sf1, &sf2, &subnet2);
		fprint_subnet_file_fmt(nof->output_file, &sf2, nof->output_fmt);
		free(sf1.routes);
		free(sf2.routes);
		return 0;
	} else {
		fprintf(stderr, "invalid objet %s after %s, expecting 'subnet' or 'file'\n", argv[2], argv[1]);
		return -1;

	}
	return 0;
}

static int run_split(int arc, char **argv, void *st_options) {
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

static int run_split_2(int arc, char **argv, void *st_options) {
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

static int run_scanf(int arc, char **argv, void *st_options) {
	int res;
	struct sto o[40];
	
        res = sto_sscanf(argv[2], argv[3], o, 40);
	if (res < 0) {
		fprintf(stderr, "no match\n");
		return res;
	}
	sto_printf("%O0 %O1 %O2 %O3 %O4 %O5 %O6 %O7\n", o, res);
	return 0;
}

static int run_help(int arc, char **argv, void *st_options) {
	usage();
	return 0;
}

static int run_version(int arc, char **argv, void *st_options) {
	printf("%s version %s by Mahmoud Basset\n", PROG_NAME, PROG_VERS);
#ifdef __DATE__
	printf("Compiled %s\n", __DATE__);
#endif
	printf("%s is licenced under the GPLv2.0\n", PROG_NAME);
	return 0;
}

static int run_confdesc(int arc, char **argv, void *st_options) {
	printf("-%s use 'st.conf' as the default configuration file\n", PROG_NAME);
	printf("-It is used to describe your CSVs or your IPAM format\n");
	printf("-format of a line file is :\n");
	printf("FIELD=VALUE\n");
	printf("-comments starts with a '#'\n\n");
	printf("-Fields you can configure :\n");

	config_file_describe();
	return 0;
}

static int run_bgpcmp(int arc, char **argv, void *st_options) {
	struct bgp_file sf1;
	struct bgp_file sf2;
	int res;

	res = load_bgpcsv(argv[2], &sf1, st_options);
	if (res < 0)
		return res;
	res = load_bgpcsv(argv[3], &sf2, st_options);
	if (res < 0)
		return res;
	compare_bgp_file(&sf1, &sf2, st_options);
	return 0;
}

static int run_bgpsortby(int arc, char **argv, void *st_options) {
	struct bgp_file sf1;
	int res;
	struct st_options *o = st_options;

	if (!strncmp(argv[2], "help", strlen(argv[2]))) {
		bgp_available_cmpfunc(stderr);
		return 0;
	}
	res = load_bgpcsv(argv[3], &sf1, st_options);
	if (res < 0)
		return res;
	res = bgp_sort_by(&sf1, argv[2]);
	if (res == -1664) {
		fprintf(stderr, "Cannot sort by '%s'\n", argv[2]);
		fprintf(stderr, "You can sort by :\n");
		bgp_available_cmpfunc(stderr);
		return res;
	}
	fprint_bgp_file_header(o->output_file);
	fprint_bgp_file(o->output_file, &sf1);
	return 0;
}

static int run_test(int arc, char **argv, void *st_options) {
	struct bgp_file sf1;
	struct bgp_file sf2;

	load_bgpcsv(argv[2], &sf1, st_options);
	load_bgpcsv(argv[3], &sf2, st_options);
	return 0;
}

static int run_gen_expr(int arc, char **argv, void *st_options) {
	struct generic_expr e;
	int res;

	init_generic_expr(&e, argv[2], int_compare);
	res = run_generic_expr(argv[2], strlen(argv[2]), &e);
	printf("res=%d\n", res);
	return 0;
}

static int run_test2(int arc, char **argv, void *st_options) {
	int res;
	struct sto o[40];

	
        res = sto_sscanf(argv[2], argv[3], o, 40);
	printf("%d \n", res);
	sto_printf("%O0 %O1 %O2 %O3 %O4 %O5 %O6 %O7\n", o, res);
	return 0;
}
/*
 * OPTION HANDLERS
 */
static int option_verbose(int arc, char **argv, void *st_options) {
	debugs_level[__D_ALL] = 1;
	return 0;
}

static int option_delim(int argc, char **argv, void *st_options) {
	struct st_options *nof = st_options;

	debug(PARSEOPTS, 2, "changing delim to :\"%s\"\n", argv[1]);
	if (strlen(argv[1]) > MAX_DELIM - 1 ) {
		fprintf(stderr, "too many delimiters, MAX %d\n", MAX_DELIM-1);
		return -1;
	}
	strcpy(nof->delim, argv[1]);
	return 0;
}

static int option_grepfield(int argc, char **argv, void *st_options) {
	struct st_options *nof = st_options;

	debug(PARSEOPTS, 2, "grepping only field %s\n", argv[1]);
	if (!isUnsignedInt(argv[1])) {
		fprintf(stderr, "invalid grep_field %s\n", argv[1]);
	} else {
		nof->grep_field = atoi(argv[1]);
	}
	return 0;

}

static int option_output(int argc, char **argv, void *st_options) {
	struct st_options *nof = st_options;

	debug(PARSEOPTS, 2, "changing ouput file to : \"%s\"\n", argv[1]);
	nof->output_file = fopen(argv[1], "w");
	if (nof->output_file == NULL) {
		fprintf(stderr, "cannot open %s for writing, redirecting to standard output\n", argv[1]);
		nof->output_file = stdout;
	}
	return 0;
}

static int option_debug(int argc, char **argv, void *st_options) {
	debug(PARSEOPTS, 3, "debug st_options : \"%s\"\n", argv[1]);
	if (!strcmp(argv[1], "help") || !strcmp(argv[1], "list")) {
		debug_usage();
		return -1;
	}
	parse_debug(argv[1]);
	return 0;
}

static int option_config(int argc, char **argv, void *st_options) {
	struct st_options *nof = st_options;

	debug(PARSEOPTS, 2, "Using configuration file : \"%s\"\n", argv[1]);
	nof->config_file = argv[1];
	return 0;
}

static int option_addr_compress(int argc, char **argv, void *st_options) {
	struct st_options *nof = st_options;
	int a;

	if (!isUnsignedInt(argv[1])) {
		fprintf(stderr, "expected an unsigned int after option '-p', but got '%s'\n", argv[1]);
		return 0;
	}
	a = atoi(argv[1]);
	if (a < 0 || a > 3) {
		fprintf(stderr, "out of bound value for option '-p', [0-2], got '%s'\n", argv[1]);
		return 0;
	}
	nof->ip_compress_mode = a;
	return 0;
}

static int option_fmt(int argc, char **argv, void *st_options) {
	struct st_options *nof = st_options;

	strxcpy(nof->output_fmt, argv[1], sizeof(nof->output_fmt));
	return 0;
}

static int option_rt(int argc, char **argv, void *st_options) {
	struct st_options *nof = st_options;

	nof->rt = 1;
	return 0;
}

static int option_ecmp(int argc, char **argv, void *st_options) {
	struct st_options *nof = st_options;

	nof->ecmp = 1;
	return 0;
}
/* ensure a core dump is generated in case of BUG
 * subnettool is bug free of course :)
 * man page says it is POSIX, let s hope so
 */
static void allow_core_dumps() {
	struct rlimit limits;
	int res;

	limits.rlim_cur = RLIM_INFINITY;
	limits.rlim_max = RLIM_INFINITY;
	res = setrlimit(RLIMIT_CORE, &limits);
	if (res == -1)
		fprintf(stderr, "couldnt set CORE dump size\n");
}

int main(int argc, char **argv) {
	struct st_options nof;
	int res;
	char *s;
	char conf_abs_path[256];

	memset(&nof, 0 , sizeof(nof));
	nof.output_file = stdout;
	nof.ip_compress_mode = 3; /* full IPv6 address compression  with IPv4 mapped/compatible support*/
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
		res = strxcpy(conf_abs_path + res, "/st.conf", sizeof(conf_abs_path) - res);
		nof.config_file = conf_abs_path;
	}
	open_config_file(nof.config_file, &nof);

	/* if delims are not set, set the default one*/
	if (strlen(nof.delim) == 0)
		strcpy(nof.delim, ";\n");
	else {
		nof.delim[strlen(nof.delim) + 1]= '\0';
		nof.delim[strlen(nof.delim)]= '\n';
	}
	if (strlen(nof.ipam_delim) == 0)
		strcpy(nof.ipam_delim, ",\n");
	else {
		nof.ipam_delim[strlen(nof.ipam_delim) + 1]= '\0';
		nof.ipam_delim[strlen(nof.ipam_delim)]= '\n';
	}
	/* if the default output format has not been set */
	if (strlen(nof.output_fmt) < 2)
		strcpy(nof.output_fmt, default_fmt);
	if (strlen(nof.bgp_output_fmt) < 2)
		strcpy(nof.bgp_output_fmt, bgp_default_fmt);

	res = generic_command_run(argc, argv, PROG_NAME, &nof);
	fclose(nof.output_file);
	exit(res);
}
