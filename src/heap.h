#ifndef HEAP_H
#define HEAP_H

struct tas {
	void **tab;
	unsigned long nr; /* nr of elements in the HEAP */
	unsigned long max_nr; /* number of malloc'd elements */
	int (*compare)(void *, void *);
	void (*print)(void *);
};

typedef struct tas TAS;

int alloc_tas(TAS *tas, unsigned long n, int (*compare)(void *v1, void *v2));
/* it is up to the caller to make sure t->tab is large enough
 * addTAS itself cannot fail but horrible things could happen if ->tab is not large enough
 */
void addTAS(TAS *t, void *elem);

/* if you are unsure, use this one
 * checking the return value is mandatory */
int addTAS_may_fail(TAS *tas, void *elem) __attribute__ ((warn_unused_result));
void *popTAS(TAS *);
void print_tas(TAS tas);

#else
#endif
