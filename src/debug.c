#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>

#include "debug.h"
#include "utils.h"

struct debug debugs[] = {
	{ "all",	__D_ALL,	"activates ALL debugs"},
	{ "timing",	__D_TIMING,	"records time to execute some functions"},
	{ "memory",	__D_MEMORY,	"trace memory allocations"  },
	{ "debug",	__D_DEBUG,	"debug DEBUG, yes we can! :)" },
	{ "parseipv6",	__D_PARSEIPV6,	"debug IPv6 parsing functions" },
	{ "parseip",	__D_PARSEIP,	"debug IPv4 parsing functions"},
	{ "addrcomp",	__D_ADDRCOMP,	"debug subnet file comparison functions"},
	{ "parseopts",	__D_PARSEOPTS,	"debug command-line and options parsing" },
	{ "paip",	__D_PAIP,	"debug IPAM parsing" },
	{ "ipam",	__D_IPAM,	"debug IPAM parsing" },
	{ "loadcsv",	__D_LOAD_CSV,	"debug Generic CSV Body parsing"},
	{ "csvheader",	__D_CSVHEADER,	"debug CSV header parsing"},
	{ "grep",	__D_GREP,	"debug network grep"},
	{ "parseroute", __D_PARSEROUTE,	"debug parsing of routes file to CSV"},
	{ "convert",	__D_PARSEROUTE,	"debug parsing of routes file to CSV"},
	{ "aggregate",	__D_AGGREGATE,	"debug subnet aggregate functions"}, 
	{ "addrremove",	__D_ADDRREMOVE,	"debug address removal" },
	{ "split",	__D_SPLIT,	"debug subnet splitting" },
	{ "tryguess",	__D_TRYGUESS,	"debug TRYGUESS" },
	{ "configfile",	__D_CONFIGFILE,	"debug config file parsing" },
	{ "fmt",	__D_FMT,	"debug FMT dynamic output" },
	{ "scanf",	__D_SCANF,	"debug st_scanf" },
	{ "bgpcmp",	__D_BGPCMP,	"debug BGP compare functions" },
	{ "expr",	__D_GEXPR,	"debug generic expression matching" },
	{ "filter",	__D_FILTER,	"debug route/BGP route filtering" },
	{0, 0}
};

char debugs_level [sizeof(debugs)/sizeof(struct debug)];
struct timeval tv_start[10]; /* nested timer values (10 levels max) */
int num_times = 0;

void list_debugs() {
	int i;
	char *zorglub;

	for (i = 0; ;i++) {
		if (debugs[i].name == NULL)
			break;
		zorglub = (debugs[i].long_desc ? (char *)debugs[i].long_desc : "No available description");
		printf("  %-12s: %s\n", debugs[i].name, zorglub);
	}
}

void debug_timing_start(int level) {
	if (debugs_level[__D_TIMING] < level || num_times == 10)
		return;
	gettimeofday(&tv_start[num_times], NULL);
	num_times++;
}
void __debug_timing_end(char *s, int level) {
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

void parse_debug(char *string) {
	int i, len;
	char *s, *save_s1;
	char s2[52];
	int debug_level;

	s = strtok_r(string, ",", &save_s1);
	if (s == NULL)
		return;
	do {
		len = strlen(s);
		debug_level = 1;
		strxcpy(s2, s, sizeof(s2));
		if (len > 1 && s2[len - 2] == ':') {
				if (isdigit(s[len - 1]))
					debug_level = s[len - 1] - '0';
				else
					debug(DEBUG, 1, "invalid debug level '%c'\n", s[len - 1]);
				s2[len - 2] = '\0';
		}
		for (i = 0; ; i++) {
			if (debugs[i].name == NULL)
				break;
			if (!strcmp(s2, debugs[i].name)) {
				debugs_level[debugs[i].num] = debug_level;;
				debug(DEBUG, 3, "adding debug flag %s, level %d\n", debugs[i].name, debug_level);
				break;
			}
		}
		s = strtok_r(NULL, ",", &save_s1);
	} while (s);
} 


void *st_malloc(unsigned long n, char *s) {
	void *ptr;

	ptr = malloc(n);
	if (ptr == NULL) {
		fprintf(stderr, "Unable to allocate %lu Kbytes for %s\n", n, s);
		return NULL;
	}
	debug(MEMORY, 3, "Allocated %lu Kbytes for %s\n", n, s);
	return ptr;
}

char  try_to_guess_delim(FILE *f) {
	char try[] = ",;/| &#-";
	int count[16];
	int i, j;
	int maxline = 0;
	int max_count = 0;
	char *s;
	char c;
	char buffer[1024];

	memset(count, 0, sizeof(count));
	rewind(f);
	
	while ((s = fgets(buffer, sizeof(buffer), f)) && maxline++ < 4 ) { 
		for (j = 0; j < strlen(buffer); j++) {
			for (i = 0; i < strlen(try); i++)
				if (try[i] == s[j]) {
					count[i]++;
					debug(TRYGUESS, 3, "%c one hit\n", try[i]);
				}
		}
	}
	
	for (i = 0; i < strlen(try) ; i++) {
		debug(TRYGUESS, 1, "%c %d\n", try[i], count[i]);
		if (count[i] > max_count) {
			max_count = count[i];
			c = try[i];
		}
	}
	return c;
}
#ifdef MAIN
void test_me() {
	
	malloc(104);
	debug(MEMORY, 1, "%d\n", 100);
	
}

int main(int argc, char **argv) {
	unsigned long long a;
	char c;
	parse_debug(argv[1]);
	test_me();
	c=try_to_guess_delim(fopen(argv[2],"r"));
	printf("delim is '%c'\n", c);
}
#endif
