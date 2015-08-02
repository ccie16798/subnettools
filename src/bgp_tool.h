#ifndef BGPTOOL_H
#define BGPTOOL_H

void fprint_bgp_route(FILE *, struct bgp_route *r);
int load_bgpcsv(char  *name, struct bgp_file *sf, struct st_options *nof);
int compare_bgp_file(const struct bgp_file *sf1, const struct bgp_file *sf2, struct st_options *o);
int bgp_sort_by(struct bgp_file *sf, char *name);

#else
#endif
