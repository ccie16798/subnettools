#ifndef IPAM_H
#define IPAM_H

#include "st_options.h"
#include "iptools.h"
#include "st_object.h"
#include "st_routes_csv.h"

#define IPAM_MAX_LINE_NUMBER (((unsigned long)0 - 1) / (2 * sizeof(struct ipam_line)))

struct ipam_line {
	struct subnet subnet;
	int ea_nr; /* number of Extensible Attributes */
	struct ipam_ea *ea;
};


struct ipam_file {
	struct ipam_line *lines;
	unsigned long nr;
	unsigned long max_nr; /* the number of routes that has been malloc'ed */
	int ea_nr; /* number of Extensible Attributes */
	char **ea; /* Extensible attributes names */
};

int alloc_ipam_file(struct ipam_file *sf, unsigned long n, int ea_nr);
void free_ipam_file(struct ipam_file *sf);
int load_ipam(char  *name, struct ipam_file *sf, struct st_options *nof);
int fprint_ipamfilter_help(FILE *out);
int ipam_file_filter(struct ipam_file *sf, char *expr);
int populate_sf_from_ipam(struct subnet_file *sf, struct ipam_file *ipam);

#else
#endif
