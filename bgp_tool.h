#ifndef BGPTOOL_H
#define BGPTOOL_H


int load_bgpcsv(char  *name, struct bgp_file *sf, struct st_options *nof);
int compare_bgp_file(struct bgp_file *sf1, struct bgp_file *sf2);

#else
#endif
