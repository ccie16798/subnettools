#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "debug.h"
#include <sys/resource.h>
#include "iptools.h"
#include "routetocsv.h"
#include "utils.h"
#include "generic_csv.h"
#include "subnet_tool.h"
#include "generic_command.h"
#include "config_file.h"


struct file_options fileoptions[] = {
	{ FILEOPT_LINE(ipam_prefix_field, struct options, TYPE_STRING), "IPAM CSV header field describing the prefix"  },
	{ FILEOPT_LINE(ipam_mask, struct options, TYPE_STRING), "IPAM CSV header field describing the prefix" },
	{ FILEOPT_LINE(ipam_comment1, struct options, TYPE_STRING), "IPAM CSV header field describing comment" },
	{ FILEOPT_LINE(ipam_comment2, struct options, TYPE_STRING),  "IPAM CSV header field describing comment" },
	{ FILEOPT_LINE(ipam_delim, struct options, TYPE_STRING),  "IPAM CSV delimitor" },
	{ "netcsv_delim", TYPE_STRING, sizeofmember(struct options, delim), offsetof(struct options, delim), "CSV delimitor" },
	{ FILEOPT_LINE(netcsv_prefix_field, struct options, TYPE_STRING), "Subnet CSV header field describing the prefix" },
	{ FILEOPT_LINE(netcsv_mask, struct options, TYPE_STRING), "Subnet CSV header field describing the mask" },
	{ FILEOPT_LINE(netcsv_comment, struct options, TYPE_STRING), "Subnet CSV header field describing the comment" },
	{ FILEOPT_LINE(netcsv_device, struct options, TYPE_STRING), "Subnet CSV header field describing the device" },
	{ FILEOPT_LINE(netcsv_gw, struct options, TYPE_STRING), "Subnet CSV header field describing the gateway" },
	{ FILEOPT_LINE(subnet_off, struct options, TYPE_INT) },
	{NULL,                  0, 0}
};


static int run_diff(int argc, char **argv, void *options);
static int run_compare(int argc, char **argv, void *options);
static int run_missing(int argc, char **argv, void *options);
static int run_paip(int argc, char **argv, void *options);
static int run_grep(int argc, char **argv, void *options);
static int run_read(int argc, char **argv, void *options);
static int run_simplify1(int argc, char **argv, void *options);
static int run_simplify2(int argc, char **argv, void *options);
static int run_common(int argc, char **argv, void *options);
static int run_addfiles(int argc, char **argv, void *options);
static int run_sort(int argc, char **argv, void *options);
static int run_sum(int argc, char **argv, void *options);
static int run_aggregate(int argc, char **argv, void *options);
static int run_help(int argc, char **argv, void *options);
static int run_version(int argc, char **argv, void *options);
static int run_confdesc(int argc, char **argv, void *options);
static int run_test(int argc, char **argv, void *options);

static int option_verbose(int argc, char **argv, void *options);
static int option_delim(int argc, char **argv, void *options);
static int option_grepfield(int argc, char **argv, void *options);
static int option_output(int argc, char **argv, void *options);
static int option_debug(int argc, char **argv, void *options);
static int option_config(int argc, char **argv, void *options);

struct st_command commands[] = {
	{ "diff",	&run_diff,	2},
	{ "compare",	&run_compare,	2},
	{ "missing",	&run_missing,	2},
	{ "paip",	&run_paip,	2},
	{ "grep",	&run_grep,	2},
	{ "read",	&run_read,	1},
	{ "simplify1",	&run_simplify1,	1},
	{ "simplify2",	&run_simplify2,	1},
	{ "common",	&run_common,	2},
	{ "addfiles",	&run_addfiles,	2},
	{ "sort",	&run_sort,	1},
	{ "sum",	&run_sum,	1},
	{ "aggregate",	&run_aggregate,	1},
	{ "help",	&run_help,	0},
	{ "version",	&run_version,	0},
	{ "confdesc",	&run_confdesc,	0},
	{ "test",	&run_test,	1},
	{NULL, 		NULL,		0}
};

struct st_command options[] = {
	{"-V",		&option_verbose,	0},
	{"-d",		&option_delim,		1},
	{"-grep_field",	&option_grepfield,	1},
	{"-o",		&option_output,		1},
	{"-D",		&option_debug,		1},
	{"-c",		&option_config,		1},
	{NULL, NULL, 0}
};

void usage() {
	printf("Usage: %s [OPTIONS] COMMAND FILES ....\n", PROG_NAME);
	printf("Files must be in  CSV format : \n");
	printf("\n");
	printf("\nCOMMAND := \n");
	printf("compare FILE1 FILE2 : compare FILE1 & FILE2, printing subnets in FILE1 INCLUDED in FILE2\n");
	printf("missing FILE1 FILE2 : prints subnets from FILE1 that are not covered by FILE2\n");
	printf("paip PAIP FILE1     : load IPAM, and print FILE1 subnet with comment extracted from IPAM\n");
	printf("sum FILE            : get total numbers of hosts included in the list of subnets; for IPv6, get total number of /64 routes\n");
	printf("grep FILE prefix    : grep FILE for prefix/mask\n");
	printf("read PARSER FILE1   : convert FILE1 to csv using parser PARSER; use '%s -r help' for available parsers \n", PROG_NAME);
	printf("diff FILE1 FILE2    : diff FILE1 & FILE2\n");
	printf("sort FILE1          : sort CSV FILE1\n");
	printf("aggregate FILE1     : sort and aggregate subnets in CSV FILE1\n");
	printf("simplify1 FILE1     : simplify CSV subnet file FILE1; duplicate or included networks are removed\n");
	printf("simplify2 FILE1     : simplify CSV subnet file FILE1; prints redundant routes that can be removed\n");
	printf("common FILE1 FILE2  : merge CSV subnet files FILE1 & FILE2; prints common routes only\n");
	printf("addfiles FILE1 FILE2: merge CSV subnet files FILE1 & FILE2; prints the sum of both files\n");
	printf("confdesc            : print a small explanation of %s configuration file\n", PROG_NAME);
	printf("help                : This HELP \n");
	printf("version             : %s version \n", PROG_NAME);
	printf("\nOPTION := \n");
	printf("-d <delim>      : change the default field delim (;) \n");
	printf("-o <file >      : write output in <file> \n");
	printf("-grep_field N   : grep field N only\n");
	printf("-D <debug>      : DEBUG MODE ; use '%s -D help' for more info\n", PROG_NAME);
	printf("-V              : verbose mode; same as '-D all:1'\n");
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
static int run_diff(int arc, char **argv, void *options) {
	int res;
	struct subnet_file sf1, sf2;
	struct options *nof = options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	if (res < 0)
		fprintf(stderr,"invalid file %s; not a CSV?\n", argv[2]);
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0)
		fprintf(stderr,"invalid file %s; not a CSV?\n", argv[3]);
	diff_files(&sf1, &sf2, nof);
	return 0;
}
static int run_compare(int arc, char **argv, void *options) {
	int res;
	struct subnet_file sf1, sf2;
	struct options *nof = options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	if (res < 0)
		fprintf(stderr,"invalid file %s; not a CSV?\n", argv[2]);
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0)
		fprintf(stderr,"invalid file %s; not a CSV?\n", argv[3]);
	compare_files(&sf1, &sf2, nof);
	return 0;
}

static int run_missing(int arc, char **argv, void *options) {
	int res;
	struct subnet_file sf1, sf2;
	struct options *nof = options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	if (res < 0)
		fprintf(stderr,"invalid file %s; not a CSV?\n", argv[2]);
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0)
		fprintf(stderr,"invalid file %s; not a CSV?\n", argv[3]);
	missing_routes(&sf1, &sf2, nof);
	return 0;
}

static int run_paip(int arc, char **argv, void *options) {
	int res;
	struct subnet_file paip, sf;
	struct options *nof = options;

	res = load_PAIP(argv[2], &paip, nof);
	if (res < 0) {
		fprintf(stderr,"invalid IPAM file %s; not a CSV?\n", argv[2]);
		return res;
	}
	res = load_netcsv_file(argv[3], &sf, nof);
	if (res < 0) {
		fprintf(stderr,"invalid file %s; not a CSV?\n", argv[3]);
		return res;
	}
	print_file_against_paip(&sf, &paip, nof);
	return 0;
}

static int run_grep(int arc, char **argv, void *options) {
	struct options *nof = options;
	int res;
	res = network_grep_file(argv[2], nof, argv[3]);
	if (res < 0)
		return res;
	return 0;
}

static int run_read(int arc, char **argv, void *options) {
	struct options *nof = options;

	runcsv(argv[2], argv[3], nof->output_file);
	return 0;
}

static int run_simplify1(int arc, char **argv, void *options) {
	int res;
	struct subnet_file sf;
	struct options *nof = options;

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
	fprint_subnet_file(sf, nof->output_file);
	free(sf.routes);
	return 0;
}

static int run_simplify2(int arc, char **argv, void *options) {
	int res;
	struct subnet_file sf;
	struct options *nof = options;

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
	fprint_subnet_file(sf, nof->output_file);
	free(sf.routes);
	return 0;
}

static int run_common(int arc, char **argv, void *options) {
	int res;
	struct subnet_file sf1, sf2, sf3;
	struct options *nof = options;

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
		fprint_subnet_file(sf3, nof->output_file);
	free(sf3.routes);
	free(sf2.routes);
	free(sf1.routes);
	if (res < 0) {
		fprintf(stderr, "Couldnt merge file %s\n", argv[2]);
		return res;
	}
	return 0;
}

static int run_addfiles(int arc, char **argv, void *options) {
	int res;
	unsigned long i, j;
	struct subnet_file sf1, sf2, sf3;
	struct options *nof = options;

	res = load_netcsv_file(argv[2], &sf1, nof);
	if (res < 0)
		return res;
	res = load_netcsv_file(argv[3], &sf2, nof);
	if (res < 0)
		return res;

	res = alloc_subnet_file(&sf3, sf1.nr + sf2.nr);
	if (res < 0)
		return res;
	debug_timing_start();
	for (i = 0; i < sf1.nr; i++)
		memcpy(&sf3.routes[i], &sf1.routes[i], sizeof(struct route));
	for (j = 0; j < sf2.nr; j++)
		memcpy(&sf3.routes[i+j], &sf2.routes[j], sizeof(struct route));
	sf3.nr = i + j;
	subnet_file_simplify(&sf3);
	fprint_subnet_file(sf3, nof->output_file);
	free(sf1.routes);
	free(sf2.routes);
	free(sf3.routes);
	debug_timing_end();
	return 0;
}


static int run_sort(int arc, char **argv, void *options) {
	int res;
	struct subnet_file sf;
	struct options *nof = options;

	res = load_netcsv_file(argv[2], &sf, nof);

	if (res < 0)
		return res;
	res = subnet_sort_ascending(&sf);
	if (res < 0) {
		fprintf(stderr, "Couldnt sort file %s\n", argv[2]);
		return res;
	}
	print_subnet_file(sf);
	free(sf.routes);
	return 0;
}
static int run_sum(int arc, char **argv, void *options) {
	int res;
	struct subnet_file sf;
	struct options *nof = options;
	unsigned long long sum;

	res = load_netcsv_file(argv[2], &sf, nof);
	if (res < 0)
		return res;
	sum = sum_subnet_file(&sf);
	fprintf(nof->output_file, "Sum : %llu\n", sum);
	free(sf.routes);
	return 0;
}

static int run_aggregate(int arc, char **argv, void *options) {
	int res;
	struct subnet_file sf;
	struct options *nof = options;

	res = load_netcsv_file(argv[2], &sf, nof);
	if (res < 0)
		return res;
	res = aggregate_route_file(&sf);
	if (res < 0) {
		fprintf(stderr, "Couldnt aggregate file %s\n", argv[2]);
		return res;
	}
	print_subnet_file(sf);
	free(sf.routes);
	return 0;
}

static int run_help(int arc, char **argv, void *options) {
	usage();
	return 0;
}

static int run_version(int arc, char **argv, void *options) {
	printf("%s version %s by Mahmoud Basset\n", PROG_NAME, PROG_VERS);
#ifdef __DATE__
	printf("Compiled %s\n", __DATE__);
#endif
	return 0;
}

static int run_confdesc(int arc, char **argv, void *options) {
	printf("-%s use 'st.conf' as the default configuration file\n", PROG_NAME);
	printf("-It is used to describe your CSVs or your IPAM format\n");
	printf("-format of a line file is :\n");
	printf("FIELD=VALUE\n");
	printf("-comments starts with a '#'\n\n");
	printf("-Fields you can configure :\n");

	config_file_describe();
	return 0;
}


static int run_test(int arc, char **argv, void *options) {
	struct subnet subnet;
	char buffer[51];
	get_single_ip(argv[2], &subnet) ;
	subnet2str(&subnet, buffer);
	printf("%s \n", buffer);
	return 0;
}
/*
 * OPTION HANDLERS
 */
static int option_verbose(int arc, char **argv, void *options) {
	debugs_level[__D_ALL] = 1;
	return 0;
}
static int option_delim(int argc, char **argv, void *options) {
	struct options *nof = options;

	debug(PARSEOPTS, 2, "changing delim to :\"%s\"\n", argv[1]);
	if (strlen(argv[1]) > MAX_DELIM - 1 ) {
		fprintf(stderr, "too many delimiters, MAX %d\n", MAX_DELIM-1);
		return -1;
	}
	strcpy(nof->delim, argv[1]);
	return 0;
}

static int option_grepfield(int argc, char **argv, void *options) {
	struct options *nof = options;

	debug(PARSEOPTS, 2, "grepping only field %s\n", argv[1]);
	if (!isUnsignedInt(argv[1])) {
		fprintf(stderr, "invalid grep_field %s\n", argv[1]);
	} else {
		nof->grep_field = atoi(argv[1]);
	}
	return 0;

}

static int option_output(int argc, char **argv, void *options) {
	struct options *nof = options;

	debug(PARSEOPTS, 2, "changing ouput file to : \"%s\"\n", argv[1]);
	nof->output_file = fopen(argv[1], "w");
	if (nof->output_file == NULL) {
		fprintf(stderr, "cannot open %s for writing, redirecting to standard output\n", argv[1]);
		nof->output_file = stdout;
	}
	return 0;
}

static int option_debug(int argc, char **argv, void *options) {
	debug(PARSEOPTS, 3, "debug options : \"%s\"\n", argv[1]);
	if (!strcmp(argv[1], "help") || !strcmp(argv[1], "list")) {
		debug_usage();
		return -1;
	}
	parse_debug(argv[1]);
	return 0;
}

static int option_config(int argc, char **argv, void *options) {
	struct options *nof = options;

	debug(PARSEOPTS, 2, "Using configuration file : \"%s\"\n", argv[1]);
	nof->config_file = argv[1];
	return 0;
}
/* ensure a core dump is generated in cae of BUG
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
	struct options nof;
	int res;

	memset(&nof, 0 , sizeof(nof));
	nof.output_file = stdout;
	allow_core_dumps();
	res = generic_parse_options(argc, argv, PROG_NAME, &nof);
	if (res < 0)
		exit(1);
	argc = argc - res;
	argv = argv + res;
	if (argc < 2)
		exit(1);
	if (nof.config_file == NULL)
		nof.config_file = "st.conf";
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
	res = generic_command_run(argc, argv, PROG_NAME, &nof);
	fclose(nof.output_file);
	exit(res);
}
