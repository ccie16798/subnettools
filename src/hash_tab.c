#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	ht->nr         = 0;
	ht->collisions = 0;
	return new_nr;
}

void hash_insert(struct hash_table *ht, struct st_bucket *bucket)
{
	unsigned h = ht->hash_fn(bucket->key, bucket->key_len) & ht->table_mask;

	if (!list_empty(&ht->tab[h]))
		ht->collisions++;
	list_add(&bucket->list, &ht->tab[h]);
	ht->nr++;
}

struct st_bucket *find_key(struct hash_table *ht, char *key, int key_len)
{
	
	unsigned h = ht->hash_fn(key, key_len) & ht->table_mask;
	struct st_bucket *b;

	if (list_empty(&ht->tab[h]))
		return NULL;
	list_for_each_entry(b, &ht->tab[h], list) {
		if (!strncmp(key, b->key, key_len))
			return b;
	}
	return NULL;
}

struct st_bucket *remove_key(struct hash_table *ht, char *key, int key_len)
{

	unsigned h = ht->hash_fn(key, key_len) & ht->table_mask;
	struct st_bucket *b;

	if (list_empty(&ht->tab[h]))
		return NULL;
	list_for_each_entry(b, &ht->tab[h], list) {
		if (!strncmp(key, b->key, key_len)) {
			list_del(&b->list);
			if (list_empty(&ht->tab[h]))
				ht->collisions--;
			ht->nr--;
			return b;
		}
	}
	return NULL;
}

int main(int argc, char **argv)
{
	struct hash_table ht;
	struct st_bucket *b;
	char buffer[64];
	int i;

	alloc_hash_tab(&ht, 10000, &djb_hash);
	for (i = 0; i < 1000; i++) {
		b = malloc(sizeof(struct st_bucket));
		sprintf(buffer, "KEY %d", i);
		b->key = strdup(buffer);
		b->key_len = strlen(b->key);
		sprintf(b->value, "VALUE %d", i);
		hash_insert(&ht, b);
	}
	printf("%d elements inserted, %d collisions\n", ht.nr, ht.collisions);
	b = find_key(&ht, "KEY 4", 5);
	if (b)
		printf("value=%s\n", b->value);

}
