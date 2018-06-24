#ifndef HEAP_H
#define HEAP_H

/* max heap NR will be really large on 64 bits */
#define HEAP_MAX_NR (((unsigned long )0 - 1) / (2 * sizeof(void *)))

struct tas {
	void **tab;
	unsigned long nr; /* nr of elements in the HEAP */
	unsigned long max_nr; /* number of malloc'd elements */
	int (*compare)(void *, void *);
	void (*print)(void *);
};

typedef struct tas TAS;

/* alloc_tas: allocate a new heap structure
 * @tas     : the heap to initialize
 * @n       : the number of elements it can hold
 * @compare : a compare function
 * returns:
 *	positive on SUCCESS
 *	negative on ENOMEM
 */
int alloc_tas(TAS *tas, unsigned long n, int (*compare)(void *v1, void *v2));

/* free_tas: free a heap structure
 * @tas  : the heap to free
 */
void free_tas(TAS *tas);

/* addTAS: add an elem to the heap
 * it is up to the caller to make sure t->tab is large enough
 * @t    : the heap to add the element
 * @elem : the element to add
 * returns:
 *	cannot fail, but horrible things will happen if heap is not large enough
 */
void addTAS(TAS *t, void *elem);

/* addTAS_mayfail: add an elem to the heap
 * @t    : the heap to add the element
 * @elem : the element to add
 * returns:
 *	positive on SUCCESS
 *	negative on ENOMEM (if it needs to realloc the heap)
 */
int addTAS_may_fail(TAS *tas, void *elem) __attribute__ ((warn_unused_result));

/* popTAS: remove one element from the heap (the best)
 * @tas   : the heap to remove from
 * returns:
 *	NULL if no more elements
 *	the best remaining element
 */
void *popTAS(TAS *tas);
void print_tas(TAS tas);

#else
#endif
