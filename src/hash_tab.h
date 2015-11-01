#ifndef HASH_TAB_H
#define HASH_TAB_H

#include "st_list.h"

struct st_bucket {
	void *key;
	int key_len;
	st_list list;
	char value[32];
};

struct stat_bucket {
	void *key;
	int key_len;
	st_list list;
	unsigned long count;
};


struct hash_table {
	unsigned max_nr; /* power of two */
	unsigned table_mask; /* table_size - 1, used to do MODULUS */
	int collisions;
	int nr;
	st_list *tab;
	unsigned (*hash_fn)(void *, int);
};


#else
#endif
