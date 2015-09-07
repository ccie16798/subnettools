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
/* clone route src into dst
 * if dst has memory alloced in EA, free them before allocating
 */
int clone_route(struct route *dst, const struct route *src);

/* clone_route_nofree:
 * clone src into dst; DOES NOT FREE DST, make sure it is not alloced
 */
int clone_route_nofree(struct route *dst, const struct route *src);

void zero_route(struct route *a);
void zero_route_ea(struct route *a);
int alloc_route_ea(struct route *r, int n);
void free_route(struct route *r);
int is_equal_gw(struct route *r1, struct route *r2);


#else
#endif
