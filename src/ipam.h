#ifndef IPAM_H
#define IPAM_H

#include "iptools.h"

struct  ipam_ea {
	char *name;
	int type:
	void *value;
};

struct ipam {
	struct subnet subnet;
	int ea_nr; /* number of Extensible Attributes */
	struct ipam_ea *ea;
};


#else
#endif
