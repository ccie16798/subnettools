/*
 * Help and Usage functions
 *
 * Copyright (C) 2015,2018 Etienne Basset <etienne POINT basset AT ensta POINT org>
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
#include "prog-main.h"
#include "st_help.h"

void usage_en_arithmetic(void)
{
	printf("Subnet arithmetic\n");
	printf("-----------------\n");
	printf("relation [IP1] [IP2]       : prints a relationship between IP1 and IP2\n");
	printf("split [S], [l1,l2,..]      : split subnet S l1 times, the result l2 times, and so on..\n");
	printf("split2 [S], [m1,m2,..]     : split subnet S with mask m1, then m2, and so on...\n");
	printf("removesub [TYPE] [O1] [S1] : remove subnet S from Object O1; if TYPE=file O1=file, if TYPE=subnet 01=subnet\n");
	printf("removefile [FILE1] [FILE2] : remove all subnets in FILE2 from FILE1\n");
	printf("ipinfo [IP|all|IPvX]       : prints information about IP, or all known subnets (all, IPv4 or IPv6)\n");
}

void usage_en_simplify(void)
{
	printf("Route file simplification\n");
	printf("-------------------------\n");
	printf("sort [FILE]           : sort FILE by prefix\n");
	printf("sortby name [FILE]    : sort FILE by (prefix|gw|mask), prefix is a tie-breaker\n");
	printf("sortby help	      : print available sort options\n");
	printf("subnetagg [FILE]      : sort and aggregate subnets in FILE; GW is not checked\n");
	printf("routeagg  [FILE]      : sort and aggregate subnets in FILE; GW is checked\n");
	printf("routesimplify1 [FILE] : simplify CSV subnet file duplicate or included networks are removed;\n");
	printf("routesimplify2 [FILE] : simplify CSV subnet file; prints redundant routes that can be removed\n");
}

void usage_en_routecompare(void)
{
	printf("Route file comparison\n");
	printf("---------------------\n");
	printf("compare [FILE1] [FILE2]    : printing subnets in FILE1 and their relation with FILE2\n");
	printf("subnetcmp [BEFORE] [AFTER] : compare files BEFORE and AFTER, showing new/removed/changed routes\n");
	printf("missing [BEFORE] [AFTER]   : prints subnets from BEFORE missing from AFTER; GW is not checked\n");
	printf("uniq [FILE1] [FILE2]       : prints unique subnets from FILE1 and FILE2\n");
	printf("common [FILE1] [FILE2]     : merge FILE1 & FILE2; prints common routes only; GW isn't checked\n");
	printf("addfiles [FILE1] [FILE2]   : merge FILE1 & FILE2; prints the sum of both files\n");
	printf("grep [FILE] [prefix]       : grep FILE for prefix/mask\n");
	printf("filter [FILE] [EXPR]       : grep netcsv FILE using regexp EXPR\n");
	printf("filter help                : prints help about filters\n");
}

void usage_en_ipam(void)
{
	printf("IPAM tools\n");
	printf("----------\n");
	printf("ipam [IPAM] [FILE]       : using IPAM, print all FILE subnets with comments extracted from IPAM\n");
	printf("ipamfilter [IPAM] [EXPR] : filter IPAM file using regexp EXPR\n");
	printf("ipamprint [IPAM]         : print IPAM; use option -ea to select Extended Attributes\n");
	printf("getea [IPAM] [FILE]      : print FILE with Extended Attributes extracted from IPAM\n");
}

void usage_en_miscellaneous(void)
{
	printf("Miscellaneous route file tools\n");
	printf("------------------------------\n");
	printf("print [FILE]        : just read & print FILE; best used with a -fmt FMT\n");
	printf("sum [IPv4FILE]      : get total number of hosts included in the list of subnets\n");
	printf("sum [IPv6FILE]      : get total number of /64 subnets included\n");
}

void usage_en_bgp(void)
{
	printf("BGP route file tools\n");
	printf("--------------------\n");
	printf("bgpprint [FILE]         : just read & print FILE; best used with -fmt FMT\n");
	printf("bgpcmp [BEFORE] [AFTER] : show what changed in BGP files BEFORE and AFTER\n");
	printf("bgpsortby [NAME] [FILE] : sort FILE by prefix, MED, etc.. prefix is a tie-breaker\n");
	printf("bgpsortby help	        : print available sort options\n");
	printf("bgpfilter [FILE] [EXPR] : grep FILE using regexp EXPR\n");
	printf("bgpfilter help          : prints help about bgp filters\n");
}

void usage_en_convert(void)
{
	printf("IP route to CSV converters\n");
	printf("--------------------------\n");
	printf("convert [PARSER] [FILE] : convert FILE to csv using parser PARSER\n");
	printf("convert help            : use '%s convert help' for available parsers\n", PROG_NAME);
}

void usage_en_debug(void)
{
	printf("DEBUG and help\n");
	printf("--------------\n");
	printf("echo [FMT] [ARG]     : try to get subnet from ARG and echo it according to FMT\n");
	printf("scanf [STRING] [FMT] : scan STRING according to scanf-like format FMT\n");
	printf("fscanf [FILE] [FMT]  : scan FILE according to scanf-like format FMT\n");
	printf("confdesc             : print a small explanation of %s configuration file\n", PROG_NAME);
	printf("help                 : this help\n");
	printf("limits               : print all limits known by %s\n", PROG_NAME);
	printf("version              : %s version\n", PROG_NAME);
}

void usage_en_options(void)
{
	printf("\nOPTIONS :=\n");
	printf("-d <delim>      : change the default field delim ';'\n");
	printf("-ea <EA1,EA2>   : load IPAM Extended Attributes; use ',' to select more\n");
	printf("-c <file>       : use config file <file>  instead of st.conf\n");
	printf("-o <file>       : write output in <file>\n");
	printf("-rt             : when converting routing table, set route type as comment\n");
	printf("-ecmp           : when converting routing table, print all routes in case of ECMP\n");
	printf("-noheader|-nh   : do not print netcsv header file\n");
	printf("-grep_field <N> : grep field N only\n");
	printf("-D <debug>      : DEBUG MODE ; use '%s -D help' for more info\n", PROG_NAME);
	printf("-fmt            : change the output format (default :%s)\n", DEFAULT_FMT);
	printf("-V              : verbose mode; same as '-D all:1'\n");
	printf("-VV             : more verbose mode; same as '-D all:2'\n");
}

void usage_en_csv(void)
{
	printf("INPUT CSV format :=\n");
	printf("- CSV files SHOULD have a CSV header describing its structure:\n"
			"(prefix, mask, GW, comment, etc...)\n");
	printf("- Subnet/routes files without a CSV header are assumed to be :\n"
			"'prefix;mask;GW;comment' or 'prefix;mask;comment'\n");
	printf("- default CSV header is 'prefix;mask;device;GW;comment'\n");
	printf("- CSV header can be changed by using the configuration file\n"
				"use '%s confdesc' for more info\n", PROG_NAME);
	printf("- IPAM CSV header MUST be described in the configuration file\n");
}

struct usages {
	const char *name;
	void (*usage)();
};

const struct usages usages_en[] = {
	{"arithmetic",	usage_en_arithmetic},
	{"simplify",	usage_en_simplify},
	{"compare",	usage_en_routecompare},
	{"ipam",	usage_en_ipam},
	{"misc",	usage_en_miscellaneous},
	{"bgp",		usage_en_bgp},
	{"convert",	usage_en_convert},
	{"debug",	usage_en_debug},
	{"options",	usage_en_options},
	{"csv",		usage_en_csv},
	{NULL, NULL}
};

/* still not ready */
const struct usages usages_fr[] = {
	{"arithmetic",	usage_en_arithmetic},
	{"simplify",	usage_en_simplify},
	{"compare",	usage_en_routecompare},
	{"ipam",	usage_en_ipam},
	{"misc",	usage_en_miscellaneous},
	{"bgp",		usage_en_bgp},
	{"convert",	usage_en_convert},
	{"debug",	usage_en_debug},
	{"options",	usage_en_options},
	{"csv",		usage_en_csv},
	{NULL, NULL}
};

const struct usages *usages[] = {usages_en, usages_fr};

void usage_brief_en(void)
{
	int i;

	printf("Commands & options are documented by section\n");
	printf("Use '%s help <section_name>' to have help about one section\n", PROG_NAME);
	printf("Available sections :\n[");
	for (i = 0; ; i++) {
		if (usages_en[i].name == NULL)
			break;
		printf("%s ", usages_en[i].name);
	}
	printf("]\n");
	printf("To get all sections, '%s help all\n", PROG_NAME);
}

void usage_all(int language)
{
	int i;
	const struct usages *u;

	if (language < LANG_MIN || language > LANG_MAX) {
		fprintf(stderr, "BUG, invalid language ID for help: %d\n", language);
		return;
	}
	u = usages[language];
	printf("Usage: %s [OPTIONS] COMMAND ARGUMENTS ....\n", PROG_NAME);
	printf("\n");
	printf("\nCOMMAND :=\n");
	printf("\n");
	for (i = 0; ; i++) {
		if (u[i].name == NULL)
			break;
		u[i].usage();
		printf("\n");
	}
}

void usage(int argc, char **argv, struct st_options *o)
{
	int i, lang;
	const struct usages *u;

	lang = LANG_EN;
	if (argv[2] == NULL) {
		usage_all(lang);
		return;
	}
	u = usages[lang];
	printf("Usage: %s [OPTIONS] [COMMAND] [ARGUMENTS] ....\n", PROG_NAME);
	printf("\n");
	for (i = 0; ; i++) {
		if (u[i].name == NULL) {
			usage_brief_en();
			return;
		}
		if (!strncmp(u[i].name, argv[2], strlen(argv[2]))) {
			u[i].usage();
			return;
		}
	}
}

void debug_usage(void)
{
	printf("Usage: %s -D Debugs [OPTIONS] [COMMAND] [FILES]\n", PROG_NAME);
	printf("Debugs looks like : symbol1:level,symbol2:level;...\n");
	printf("symbol is the  symbol to  debug, increasing level gives more info, ie :\n");
	printf("level 1 : get more infos on errors (badly formatted file, bad IP etc...)\n");
	printf("level 4+ : print  internal program debug info ; wont tell you anything without source code\n");
	printf("level 9 : if implemented, print info for each loop of the program\n");
	printf("Example 1 :\n");
	printf("'%s -D memory:1,parseopts:2,trucmuche:3 simplify1 MYFILE'\n", PROG_NAME);
	printf("Example 2 :\n");
	printf("'%s -D loadcsv:3 sort MYFILE' will tell you every invalid line in your file.\n\n", PROG_NAME);
	printf("Available symbols :\n");
	list_debugs();
}
