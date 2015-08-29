#ifndef ST_ROUTE_H
#define ST_ROUTE_H

#include "iptools.h"
#include "ipam.h"

struct route {
	struct subnet subnet;
	char device[32];
	struct ip_addr gw;
	char comment[128];
	int ea_nr; /* number of EA */
	struct ipam_ea *ea; /* Extended Attributes */
};
void copy_route(struct route *a, const struct route *b);
void zero_route(struct route *a);
void zero_route_ea(struct route *a);
int alloc_route_ea(struct route *r, int n);
void free_route(struct route *r);
int is_equal_gw(struct route *r1, struct route *r2);


#else
#endif
