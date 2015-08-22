#ifndef IPAM_H
#define IPAM_H

#include "iptools.h"
#include "st_object.h"

#ifndef SIZE_T_MAX
#define SIZE_T_MAX ((size_t)0 - 1)
#endif

struct  ipam_ea {
	char *name;
	char *value;
};

struct ipam {
	struct subnet subnet;
	int ea_nr; /* number of Extensible Attributes */
	struct ipam_ea *ea;
};


struct ipam_file {
	struct ipam *routes;
	unsigned long nr;
	int ea_nr; /* number of Extensible Attributes */
	unsigned long max_nr; /* the number of routes that has been malloced */

};

int alloc_ipam_file(struct ipam_file *sf, unsigned long n);


#else
#endif
