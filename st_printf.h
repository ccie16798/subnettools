#ifndef ST_PRINTF_H
#define ST_PRINTF_H

/* simple and dummy print a route */
void fprint_route(const struct route *r, FILE *output, int compress_level);
/* print a route 'r' according to format 'fmt' into output */
void fprint_route_fmt(const struct route *r, FILE *output, const char *fmt);

/* subnettool variants of sprintf, fprintf and printf
 * fmt understand the following types :
 *
 * %I : an IP (v4 or v6)   input variable is struct subnet *
 * %m : a subnet mask      input variable is struct subnet *
 * %M : a subnet mask (ddn)
 */
int st_sprintf(char *out, const char *fmt, ...);
void st_fprintf(FILE *f, const char *fmt, ...);
void st_printf(const char *fmt, ...);

void print_route(const struct route *r, FILE *output, int comp_level);
void fprint_route_fmt(const struct route *r, FILE *output,  const char *fmt);
void print_subnet_file(const struct subnet_file *sf, int comp_level);
void fprint_subnet_file(const struct subnet_file *sf, FILE *output, int comp_level);
void fprint_subnet_file_fmt(const struct subnet_file *sf, FILE *output, const char *fmt);

#else
#endif
