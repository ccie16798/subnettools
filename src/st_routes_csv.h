#ifndef ST_HANDLE_CSV_FILES_H
#define ST_HANDLE_CSV_FILES_H

#include "st_options.h"
#include "st_routes.h"

#define SF_MAX_ROUTES_NUMBER (((unsigned long)0 - 1) / (2 * sizeof(struct route)))

struct subnet_file {
	struct route *routes;
	unsigned long nr;
	unsigned long max_nr; /* the number of routes that has been malloced */
	int ea_nr;
	struct ipam_ea *ea;
};

struct bgp_file {
	struct bgp_route *routes;
	unsigned long nr;
	unsigned long max_nr; /* the number of routes that has been malloced */
};

int alloc_subnet_file(struct subnet_file *sf, unsigned long n);
void free_subnet_file(struct subnet_file *sf);

int load_netcsv_file(char *name, struct subnet_file *sf, struct st_options *nof);
int load_ipam_no_EA(char  *name, struct subnet_file *sf, struct st_options *nof);

int alloc_bgp_file(struct bgp_file *sf, unsigned long n);
void free_bgp_file(struct bgp_file *sf);
int load_bgpcsv(char  *name, struct bgp_file *sf, struct st_options *nof);

#else
#endif
