/*
 * generic HEAP implementation
 *
 * Copyright (C) 1999-2014 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "heap.h"
#include "debug.h"
#include "st_memory.h"

#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

/* 3 not used functions, commented to silence warnings
 * But i prefer to let them here
 *x/
static inline void *father(TAS t, int n) {
	return t.tab[(n - 1) / 2];
}
static inline void *leftSon(TAS t, int n) {
	return t.tab[2 * n + 1];
}
static inline void *rightSon(TAS t, int n) {
	return t.tab[2 * n + 2];
}
*/
#ifndef SIZE_T_MAX
#define SIZE_T_MAX ((size_t)0 - 1)
#endif
int alloc_tas(TAS *tas, unsigned long n, int (*compare)(void *v1, void *v2)) {
	if (n > (SIZE_T_MAX / sizeof(void *) - 1)) {
		fprintf(stderr, "error: too much memory requested for heap\n");
		tas->tab = NULL;
		tas->nr = 0;
		return -1;
	}

	tas->tab = st_malloc(n * sizeof(void *), "heap");
	if (tas->tab == NULL) {
		tas->nr = 0;
		return -1;
	}
	tas->nr      = 0;
	tas->max_nr  = n;
	tas->compare = compare;
	tas->print   = NULL;
	return 1;
}

void free_tas(TAS *tas) {
	free(tas->tab);
	total_memory -= tas->max_nr * sizeof(void *);
	debug(MEMORY, 5, "Total memory: %lu\n", total_memory);
	tas->tab = NULL;
	tas->nr = tas->max_nr = 0;
}

void addTAS(TAS *tas, void *el) {
	unsigned long n, n2;

	n = tas->nr++;
	tas->tab[n] = el; /*insert element at the end */
	n2 = n;

	while (n) { /* move it up */
		n2--;
		n2 /= 2;
		if (tas->compare(tas->tab[n], tas->tab[n2])) { /* if son  better than father swap */
			swap(tas->tab[n2], tas->tab[n]);
			n = n2; /* move to father */
			continue;
		}
		break;
	}
}

int addTAS_may_fail(TAS *tas, void *el) {
	void **truc;

	if (tas->nr == tas->max_nr - 1) {
		if (tas->max_nr * 2 >  (SIZE_T_MAX / sizeof(void *))) {
			fprintf(stderr, "error: too much memory requested for heap\n");
			return -1;
		}
		truc = st_realloc(tas->tab, sizeof(void *) * tas->max_nr * 2,
				sizeof(void *) * tas->max_nr,
				 "heap");
		if (truc == NULL)
			return -1;
		tas->max_nr *= 2;
		tas->tab = truc;
	}
	addTAS(tas, el);
	return 0;
}

void *popTAS(TAS *tas) {
	unsigned long n, i = 0, i2;
	void *res = tas->tab[0];

	if (tas->nr == 0)
		return NULL;
	n = tas->nr - 1;
	tas->nr--;
	tas->tab[0] = tas->tab[n]; /* first replace head with last element, then lets get it down */

	while (1) { /* move down */
		i2 = 2 * i + 1;
		if (i2 == n) { /* empty right son */
			if (tas->compare(tas->tab[i], tas->tab[i2])) /* if father is better, stop */
                                break;
			else
				swap(tas->tab[i2], tas->tab[i]); /* swap father & left son */
			break;
		}
		if (i2 > n) /* no more sons */
			break;

		/* heap[2i+1] = left son, heap[2i+2] = right son */
		if (tas->compare(tas->tab[i2], tas->tab[i2 + 1])) {
			if (tas->compare(tas->tab[i], tas->tab[i2])) /* if father is better, stop */
				break;
			swap(tas->tab[i2], tas->tab[i]); /* swap father & left son */
			i = i2;
		} else {
			if (tas->compare(tas->tab[i], tas->tab[i2 + 1]))/* if father is better, stop */
				break;
			swap(tas->tab[i2+1], tas->tab[i]); /* wap father & right son */
			i = i2 + 1;
		}
	}
	return res;
}

void print_tas(TAS tas) {
	unsigned long i;

	if (tas.print == NULL)
		return;

	for (i = 0; i < tas.nr; i++)  {
		if (i)
			tas.print(tas.tab[(i - 1) / 2]);
		else
			printf("nr %lu ", tas.nr);
		printf(" : ");

		tas.print(tas.tab[i]);
		printf(" %d \n", tas.compare(tas.tab[i], tas.tab[(i - 1) / 2]));
	}
	printf("\n");
}
#ifdef TEST_TAS
static int compare_int(void *a, void *b) {
	int a1, b1;
	a1 = *((int *)a);
	b1 = *((int *)b);
	return (a1 < b1);
}

static void printint(void *i) {
	printf("%d", *((int *)i));
}

int main(int argc, char **argv) {
	TAS t;
	int i;
	int truc [] = {1008, 45, 56,76, 7 , 5, 8, 127, 129, 2, 5, 8, 8, 1, 2, 9, 25, 48 ,3, 2, 100, 200, 41, 50, 54, 67,88, 89, 101, 150, 123, 45, 67};
	int j;

	t.tab = (void **)malloc(100 * sizeof(void *));
	t.compare = &compare_int;
	t.print = &printint;
	t.nr = 0;
	for (i = 0; i < sizeof(truc)/4; i++) {
		addTAS(&t, (void *)&truc[i]);
	}
	print_tas(t);
	for (i = 0; i < sizeof(truc)/4; i++) {
		j = *((int *)popTAS(&t));
		printf("%d\n", j);
	}
	printf("\n");
}
#endif
