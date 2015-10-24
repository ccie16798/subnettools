#ifndef DEBUG_H
#define DEBUG_H

#define __D_ALL		0
#define __D_TIMING	1
#define __D_MEMORY   	2
#define __D_LOADFILE 	3  /* UNUSED */
#define __D_PARSEIPV6	4
#define __D_PARSEOPTS	5
#define __D_PAIP	6
#define __D_IPAM	6
#define __D_LOAD_CSV	7
#define __D_PARSEIP	8
#define __D_GREP	9
#define __D_ADDRCOMP	10
#define __D_PARSEROUTE	11
#define __D_DEBUG    	13
#define __D_TRYGUESS 	14
#define __D_CONFIGFILE  15
#define __D_CSVHEADER   16
#define __D_FMT		17
#define __D_SCANF	18
#define __D_AGGREGATE	30
#define __D_ADDRREMOVE	31
#define __D_SPLIT	32
#define __D_BGPCMP	40
#define __D_GEXPR	50
#define __D_FILTER	51
#define __D_LIST	55
#define __D_MAX		100

#define debug(__EVENT, __DEBUG_LEVEL, __FMT...) \
	do { \
		int ___x = (__D_##__EVENT); \
		if (debugs_level[___x] >= __DEBUG_LEVEL || debugs_level[__D_ALL] >= __DEBUG_LEVEL) { \
			fprintf(stderr,"%s : ", __FUNCTION__  ); \
			fprintf(stderr, __FMT); \
		} \
	} while (0);
struct debug {
	const char *name;
	unsigned num;
	const char *long_desc;
};

extern char debugs_level [];

void list_debugs();
void parse_debug (char *string);
void debug_timing_start(int level);
void __debug_timing_end(const char *s, int level);
#define debug_timing_end(__level) __debug_timing_end(__FUNCTION__, __level)

#else
#endif
