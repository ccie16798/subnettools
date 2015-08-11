#ifndef BGPTOOL_H
#define BGPTOOL_H

int fprint_bgp_route(FILE *, struct bgp_route *r);
void fprint_bgp_file_header(FILE *out);
void fprint_bgp_file(FILE *output, struct bgp_file *bf);
int load_bgpcsv(char  *name, struct bgp_file *sf, struct st_options *nof);
int compare_bgp_file(const struct bgp_file *sf1, const struct bgp_file *sf2, struct st_options *o);
int bgp_sort_by(struct bgp_file *sf, char *name);
void bgp_available_cmpfunc(FILE *out);

/* filter BGP CSV files with a regular expression */
int bgp_filter(struct bgp_file *sf, char *expr);

#else
#endif
