/*
 * generic HEAP implementation
 *
 * Copyright (C) 1999-2018 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include "heap.h"
#include "debug.h"
#include "st_memory.h"

#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

/* 3 not used functions, commented to silence warnings
 * But i prefer to let them here
 *
static inline void *father(TAS t, int n)
{
	return t.tab[(n - 1) / 2];
}
static inline void *leftSon(TAS t, int n)
{
	return t.tab[2 * n + 1];
}
static inline void *rightSon(TAS t, int n)
{
	return t.tab[2 * n + 2];
}
*/

int alloc_tas(TAS *tas, unsigned long n, int (*compare)(void *v1, void *v2))
{
	if (n > HEAP_MAX_NR) {
		fprintf(stderr, "error: too much memory requested for heap\n");
		tas->tab = NULL;
		tas->nr  = 0;
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

void free_tas(TAS *tas)
{
	st_free(tas->tab, tas->max_nr * sizeof(void *));
	debug(MEMORY, 5, "Total memory: %lu\n", total_memory);
	tas->tab = NULL;
	tas->nr = tas->max_nr = 0;
}

void addTAS(TAS *tas, void *el)
{
	unsigned long n, father;
    int (*func)(void *, void *) = tas->compare;
	n = tas->nr++;
	tas->tab[n] = el; /*insert element at the end */
	father = n;

	while (n) { /* move it up */
		father--;
		father >>= 1; /* father of current n */
		/* if son is better than father then swap */
		if (func(tas->tab[n], tas->tab[father])) {
			swap(tas->tab[father], tas->tab[n]);
			n = father; /* move current to father */
			continue;
		}
		break;
	}
}

int addTAS_may_fail(TAS *tas, void *el)
{
	void **truc;

	if (tas->nr == tas->max_nr - 1) {
		if (tas->max_nr * 2 > HEAP_MAX_NR) {
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

void *popTAS(TAS *tas)
{
	unsigned long n, i = 0, left_son, right_son;
	void *res = tas->tab[0];
    int (*func)(void *, void *) = tas->compare;

	if (tas->nr == 0)
		return NULL;
	tas->nr--;
	n = tas->nr;
	/* first replace head with last element, then lets get it down */
	tas->tab[0] = tas->tab[n];

	while (1) { /* move down */
		left_son = i << 1;
		left_son++; /* left son of i */
		if (left_son > n) /* no more sons */
			break;
		if (left_son == n) { /* no right son */
			/* if father is better, stop */
			if (func(tas->tab[i], tas->tab[left_son]))
				break;
			swap(tas->tab[left_son], tas->tab[i]); /* swap father & left son */
			break;
		}
		/* heap[2i+1] = left son, heap[2i+2] = right son */
        right_son = left_son + 1;
		if (func(tas->tab[left_son], tas->tab[right_son])) {
			/* if father is better, stop */
			if (func(tas->tab[i], tas->tab[left_son]))
				break;
			swap(tas->tab[left_son], tas->tab[i]); /* swap father & left son */
			i = left_son;
		} else {
			/* if father is better, stop */
			if (func(tas->tab[i], tas->tab[right_son]))
				break;
			swap(tas->tab[right_son], tas->tab[i]); /* swap father & right son */
			i = right_son;
		}
	}
	return res;
}

void print_tas(TAS tas)
{
	unsigned long i;

	if (tas.print == NULL)
		return;

	for (i = 0; i < tas.nr; i++)  {
        if (i)
			tas.print(tas.tab[(i - 1) / 2]);
		else {
			printf("nr %lu ", tas.nr);
            continue;
        }
		printf(" : ");

		tas.print(tas.tab[i]);
		printf(" %d\n", tas.compare(tas.tab[i], tas.tab[(i - 1) / 2]));
	}
	printf("\n");
}
#ifdef TEST_TAS
static int compare_int(void *a, void *b)
{
	int a1, b1;

	a1 = *((int *)a);
	b1 = *((int *)b);
	return (a1 < b1);
}

static void printint(void *i)
{
	printf("%d", *((int *)i));
}

int main(int argc, char **argv)
{
	TAS t;
	int i;
	int static_truc[] = {1008, 45, 56, 76, 7, 5, 8, 127, 129, 2, 5, 8,
		8, 1, 2, 9, 25, 48, 3, 2, 100, 200, 41, 50, 54, 67,
		88, 89, 101, 150, 123, 45, 67};
    int * truc;
	int j;
    unsigned int size = 100;
    char *s;
    if (argc > 1) { /* dynamic from cli */
        size = atoi(argv[1]);
        srand(time(NULL));
        truc = malloc(size * sizeof(int));
        for (i = 0; i < size; i++){
                truc[i] = rand() % (4*size);
        }
    } else {
        size = sizeof(static_truc) / sizeof(int);
        truc = static_truc;
    }
	t.tab = (void **)malloc(size * sizeof(void *));
	t.compare = &compare_int;
	t.print = &printint;
	t.nr = 0;
	for (i = 0; i < size; i++)
		addTAS(&t, (void *)&truc[i]);
    //print_tas(t);
	for (i = 0; i < size; i++) {
		j = *((int *)popTAS(&t));
		printf("%d\n", j);
	}
	printf("\n");
    exit(0);
}
#endif
