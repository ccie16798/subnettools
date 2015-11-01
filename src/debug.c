#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>

#include "debug.h"
#include "utils.h"

struct debug debugs[] = {
	{ "all",	__D_ALL,	"activates ALL debugs" },
	{ "timing",	__D_TIMING,	"records time to execute some functions" },
	{ "memory",	__D_MEMORY,	"trace memory allocations" },
	{ "list",	__D_LIST,	"debug linked-list operations" },
	{ "hash",	__D_HASHT,	"debug Hash Table" },
	{ "debug",	__D_DEBUG,	"debug DEBUG, yes we can! :)" },
	{ "parseipv6",	__D_PARSEIPV6,	"debug IPv6 parsing functions" },
	{ "parseip",	__D_PARSEIP,	"debug IPv4 parsing functions" },
	{ "addrcomp",	__D_ADDRCOMP,	"debug subnet file comparison functions" },
	{ "parseopts",	__D_PARSEOPTS,	"debug command-line and options parsing" },
	{ "paip",	__D_PAIP,	"debug IPAM parsing" },
	{ "ipam",	__D_IPAM,	"debug IPAM parsing" },
	{ "loadcsv",	__D_LOAD_CSV,	"debug Generic CSV Body parsing" },
	{ "csvheader",	__D_CSVHEADER,	"debug CSV header parsing" },
	{ "grep",	__D_GREP,	"debug network grep" },
	{ "parseroute", __D_PARSEROUTE,	"debug parsing of routes file to CSV" },
	{ "convert",	__D_PARSEROUTE,	"debug parsing of routes file to CSV" },
	{ "aggregate",	__D_AGGREGATE,	"debug subnet aggregate functions" },
	{ "addrremove",	__D_ADDRREMOVE,	"debug address removal" },
	{ "split",	__D_SPLIT,	"debug subnet splitting" },
	{ "configfile",	__D_CONFIGFILE,	"debug config file parsing" },
	{ "fmt",	__D_FMT,	"debug FMT dynamic output" },
	{ "scanf",	__D_SCANF,	"debug st_scanf" },
	{ "bgpcmp",	__D_BGPCMP,	"debug BGP compare functions" },
	{ "expr",	__D_GEXPR,	"debug generic expression matching" },
	{ "filter",	__D_FILTER,	"debug route/BGP route filtering" },
	{0, 0}
};

char debugs_level[__D_MAX];
struct timeval tv_start[10]; /* nested timer values (10 levels max) */
int num_times;

void list_debugs(void)
{
	int i;
	char *zorglub;

	for (i = 0; ; i++) {
		if (debugs[i].name == NULL)
			break;
		zorglub = (debugs[i].long_desc ?
				(char *)debugs[i].long_desc : "No available description");
		printf("  %-12s: %s\n", debugs[i].name, zorglub);
	}
}

void debug_timing_start(int level)
{
	if (debugs_level[__D_TIMING] < level || num_times == 10)
		return;
	gettimeofday(&tv_start[num_times], NULL);
	num_times++;
}

void __debug_timing_end(const char *s, int level)
{
	struct timeval tv_end;
	time_t sec, msec;

	if (debugs_level[__D_TIMING] < level || num_times == 10)
		return;
	num_times--;

	gettimeofday(&tv_end, NULL);
	if (tv_end.tv_usec < tv_start[num_times].tv_usec) {
		sec  = tv_end.tv_sec - tv_start[num_times].tv_sec - 1;
		msec = (1000000 + tv_end.tv_usec - tv_start[num_times].tv_usec) / 1000;
	} else {
		sec  = tv_end.tv_sec - tv_start[num_times].tv_sec;
		msec = (tv_end.tv_usec - tv_start[num_times].tv_usec) / 1000;
	}
	fprintf(stderr, "%s : elapsed time %lu sec %lu millisec\n", s, sec, msec);
}

void parse_debug(char *string)
{
	int i, len;
	char *s, *save_s1;
	char s2[52];
	int debug_level;
	int res;

	s = strtok_r(string, ",", &save_s1);
	if (s == NULL)
		return;
	do {
		debug_level = 1;
		res = strxcpy(s2, s, sizeof(s2));
		len = strlen(s);
		if (res >= sizeof(s2)) {
			fprintf(stderr, "debug string too long\n");
			return;
		}
		if (len > 1 && s2[len - 2] == ':') {
			if (isdigit(s[len - 1]))
				debug_level = s[len - 1] - '0';
			else
				debug(DEBUG, 1, "invalid debug level '%c'\n",
						s[len - 1]);
			s2[len - 2] = '\0';
		}
		for (i = 0; ; i++) {
			if (debugs[i].name == NULL)
				break;
			if (!strcmp(s2, debugs[i].name)) {
				debugs_level[debugs[i].num] = debug_level;
				debug(DEBUG, 3, "adding debug flag %s, level %d\n",
						debugs[i].name, debug_level);
				break;
			}
		}
		s = strtok_r(NULL, ",", &save_s1);
	} while (s);
}
