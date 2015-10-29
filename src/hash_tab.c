#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "debug.h"
#include "hash_tab.h"
#include "st_memory.h"

unsigned djb_hash(void *key, int len)
{
	unsigned char *s = key;
	unsigned h;
	int i;

	h = *s;
	for (i = 1; i < len; i++)
		h += (32 * h + s[i]);
	return h;
}

unsigned djb_hash_original(void *key, int len)
{
	unsigned char *s = key;
	unsigned h = 0;
	int i;

	for (i = 0; i < len; i++)
		h = (33 * h + s[i]);
	return h;
}

/* http://tools.ietf.org/html/draft-eastlake-fnv-03 */
#define FNV_Prime  16777619U
#define FNV_Offset  2166136261U

unsigned fnv_hash(void *key, int len)
{
	unsigned h = FNV_Offset;
	unsigned char *s = key;
	int i = 0;

	for (i = 0; i < len; i++)
		h = FNV_Prime * (h ^ s[i]);
	return h;
}

int alloc_hash_tab(struct hash_table *ht, unsigned long nr, unsigned (*hash)(void *, int))
{
	unsigned long new_nr;
	unsigned long i;

	if (nr == 0)
		return -2;
	new_nr = nextPow2_32(nr);
	ht->tab = st_malloc(new_nr * sizeof(struct st_list), "hash table");
	if (ht->tab == NULL)
		return -1;
	for (i = 0; i < new_nr; i++)
		init_list(&ht->tab[i]);
	ht->max_nr     = new_nr;
	ht->table_mask = new_nr - 1;
	ht->hash_fn    = hash;
	return new_nr;
}

void hash_insert(struct hash_table *ht, struct st_bucket *bucket)
{
	unsigned h = ht->hash_fn(bucket->key, bucket->key_len) & ht->table_mask;	
	
	list_add(&bucket->list, ht->table[h]);
}
