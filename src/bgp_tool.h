#ifndef BGPTOOL_H
#define BGPTOOL_H

#include "st_handle_csv_files.h"
int fprint_bgp_route(FILE *, struct bgp_route *r);
void zero_bgproute(struct bgp_route *a);
void copy_bgproute(struct bgp_route *a, const struct bgp_route *b);
void fprint_bgp_file_header(FILE *out);
void fprint_bgp_file(FILE *output, struct bgp_file *bf);
int compare_bgp_file(const struct bgp_file *sf1, const struct bgp_file *sf2, struct st_options *o);
int bgp_sort_by(struct bgp_file *sf, char *name);
void bgp_available_cmpfunc(FILE *out);

/* filter BGP CSV files with a regular expression */
int bgp_filter(struct bgp_file *sf, char *expr);

int fprint_bgpfilter_help(FILE *out);
#else
#endif
