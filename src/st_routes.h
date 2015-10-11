#ifndef ST_ROUTE_H
#define ST_ROUTE_H

#include "iptools.h"
#include "st_ea.h"

struct route {
	struct subnet subnet;
	char device[32];
	struct ip_addr gw;
	int ea_nr; /* number of EA */
	struct ipam_ea *ea; /* Extended Attributes */
};

/* copy_route can be used if it is just moving route from one container to another
 * clone_route MUST BE used if the src route is still referenced
 */
void copy_route(struct route *dst, const struct route *src);
/* clone src into dest
 * if dest had alloc'ed buffer, free tham
 *
 * returns:
 * 	1  on success
 * 	-1 on failure (ENOMEM)
 */
int clone_route(struct route *dst, const struct route *src);

/* clone src into dest
 * doesnt free dest buffers, so make sure it hasnt
 *
 * returns:
 * 	1  on success
 * 	-1 on failure (ENOMEM)
 */
int clone_route_nofree(struct route *dst, const struct route *src);

void zero_route(struct route *a);
void zero_route_ea(struct route *a);

/*
 * alloc_route_ea
 * Alloc memory for a route Extended Attributes
 * @r	: the route
 * @n	: the number of Extended Attributes to alloc
 * returns:
 * 	1  on SUCCESS
 * 	-1 on failure (ENOMEM)
 */
int alloc_route_ea(struct route *r, int n);

/*
 * realloc_route_ea
 * realloc memory for a route Extended Attributes
 * @r	: the route
 * @n	: the new number of Extended Attributes to alloc
 * returns:
 * 	1  on SUCCESS
 */
int realloc_route_ea(struct route *r, int new_n);
void free_route(struct route *r);
int is_equal_gw(struct route *r1, struct route *r2);


#else
#endif
