#ifndef ST_ROUTE_H
#define ST_ROUTE_H

#include "iptools.h"
#include "st_ea.h"

#define ST_MAX_DEVNAME_LEN 32

struct route {
	struct subnet subnet;
	char device[ST_MAX_DEVNAME_LEN];
	struct ip_addr gw;
	int ea_nr; /* number of EA */
	struct ipam_ea *ea; /* Extended Attributes */
};


static inline void zero_route(struct route *a)
{
	memset(a, 0, sizeof(struct route));
}

/* __init_route: set only required fields to zero
 * @a  : the route to init
 */
static inline void __init_route(struct route *a)
{
	a->subnet.ip_ver = 0;
	a->subnet.mask   = 0;
	a->gw.ip_ver     = 0;
	a->device[0]     = '\0';
}
/* copy_route can be used if it is just moving route from one container to another
 * clone_route MUST BE used if the src route is still referenced
 */
static inline void copy_route(struct route *a, const struct route *b)
{
	memcpy(a, b, sizeof(struct route));
}
/* clone src into dest, allocating new buffer to dst
 * if dest had alloc'ed buffer, free them
 * returns:
 *	1  on success
 *	-1 on failure (ENOMEM)
 */
int clone_route(struct route *dst, const struct route *src);

/* clone src into dest
 * doesnt free dest buffers, so make sure it hasnt
 * returns:
 *	1  on success
 *	-1 on failure (ENOMEM)
 */
int clone_route_nofree(struct route *dst, const struct route *src);

void zero_route_ea(struct route *a);

/*
 * alloc_route_ea
 * Alloc memory for a route Extended Attributes
 * @r	: the route
 * @n	: the number of Extended Attributes to alloc
 * returns:
 *	1  on SUCCESS
 *	-1 on failure (ENOMEM)
 */
int alloc_route_ea(struct route *r, int n);

/*
 * realloc_route_ea
 * realloc memory for a route Extended Attributes
 * @r	: the route
 * @n	: the new number of Extended Attributes to alloc
 * returns:
 *	1  on SUCCESS
 *	-1 onf failure (ENOME)
 */
int realloc_route_ea(struct route *r, int new_n);
void free_route(struct route *r);
int is_equal_gw(struct route *r1, struct route *r2);


#else
#endif
