#ifndef ST_PRINTF_H
#define ST_PRINTF_H
#include "st_object.h"

/* simple and dummy print a route */
void fprint_route(FILE *output, const struct route *r, int compress_level);
/* print a route 'r' according to format 'fmt' into output */
int fprint_route_fmt(FILE *output, const struct route *r, const char *fmt);

/*
 * print bgp_route 'r' with format 'fmt' to 'output'
 * if route == NULL, it will prints bgp_file HEADER
 */
int fprint_bgproute_fmt(FILE *output, const struct bgp_route *r, const char *fmt);

/* subnettool variants of sprintf, fprintf and printf
 * fmt understand the following types :
 *
 * %a : an IP (v4 or v6)   input variable is 'struct ip_addr'
 * %I : an IP (v4 or v6)   input variable is 'struct subnet'
 * %m : a subnet mask      input variable is 'struct subnet'
 * %M : a subnet mask (ddn)
 */
int st_snprintf(char *out, size_t n, const char *fmt, ...);
int st_fprintf(FILE *f, const char *fmt, ...);
int st_printf(const char *fmt, ...);

/* object-based variant; use %Oxx to print xx element from a previously filled sto list
 *
 */
int sto_snprintf(char *out, size_t len, const char *fmt, struct sto *o, int max_o, ...);
int sto_fprintf(const char *fmt, struct sto *o, int max_o, ...);
int sto_printf(const char *fmt, struct sto *o, int max_o, ...);

void print_route(FILE *out, const struct route *r, int comp_level);
void print_subnet_file(const struct subnet_file *sf, int comp_level);
void fprint_subnet_file(FILE *output, const struct subnet_file *sf, int comp_level);
void fprint_subnet_file_fmt(FILE *output, const struct subnet_file *sf, const char *fmt);

void fprint_bgp_file_fmt(FILE *output, const struct bgp_file *sf, const char *fmt);
void print_bgp_file_fmt(const struct bgp_file *sf, const char *fmt);

#define st_debug(__EVENT, __DEBUG_LEVEL, __FMT...) \
	do { \
		int ___x = (__D_##__EVENT); \
		if (debugs_level[___x] >= __DEBUG_LEVEL || debugs_level[__D_ALL] >= __DEBUG_LEVEL) { \
			st_fprintf(stderr,"%s : ", __FUNCTION__  ); \
			st_fprintf(stderr, __FMT); \
		} \
	} while (0);


#else
#endif
