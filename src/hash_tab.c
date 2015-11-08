/*
 * generic HASH TABLE functions (stat hash table)
 * hash functions alogrithms from DJ Bernstein & Fowler-Noll-Vo
 *
 * Copyright (C) 2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
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
#define FNV_Offset 2166136261U

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

struct stat_bucket *new_stat_bucket(char *key, int key_len)
{
	struct stat_bucket *sb;

	sb = st_malloc(sizeof(struct stat_bucket), "Stat bucket");
	if (sb == NULL)
		return NULL;
	sb->key = st_strdup(key);
	if (sb->key == NULL) {
		st_free(sb, sizeof(struct stat_bucket));
		return NULL;
	}
	sb->key_len = key_len;
	sb->count   = 1;
	init_list(&sb->list);
	return sb;
}

void free_stat_bucket(struct stat_bucket *sb)
{
	list_del(&sb->list);
	st_free_string(sb->key);
	st_free(sb, sizeof(struct stat_bucket));
}

void free_stat_hash_table(struct hash_table *ht)
{
	unsigned i;
	struct stat_bucket *sb;

	hlist_for_each_entry(sb, ht, i)
		free_stat_bucket(sb);
	st_free(ht->tab, ht->max_nr * sizeof(struct st_list));
	memset(ht, 0, sizeof(struct hash_table));
}

void insert_bucket(struct hash_table *ht, struct st_bucket *bucket)
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
		if (!memcmp(key, b->key, key_len))
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
		if (!memcmp(key, b->key, key_len)) {
			list_del(&b->list);
			if (list_empty(&ht->tab[h]))
				ht->collisions--;
			ht->nr--;
			return b;
		}
	}
	return NULL;
}

struct stat_bucket *increase_key_stat(struct hash_table *ht, char *key, int key_len)
{
	unsigned h = ht->hash_fn(key, key_len) & ht->table_mask;
	struct stat_bucket *sb;

	if (list_empty(&ht->tab[h])) {
		debug(HASHT, 5, "Insert key:%s\n", key);
		sb = new_stat_bucket(key, key_len);
		if (sb == NULL)
			return NULL;
		list_add(&sb->list, &ht->tab[h]);
		ht->nr++;
		return sb;
	}
	list_for_each_entry(sb, &ht->tab[h], list) {
		if (!memcmp(key, sb->key, key_len)) {
			sb->count++;
			debug(HASHT, 5, "Key %s already there, increase count to %lu\n",
					key, sb->count);
			return sb;
		}
	}
	debug(HASHT, 5, "Insert key:%s\n", key);
	sb = new_stat_bucket(key, key_len);
	if (sb == NULL)
		return NULL;
	list_add(&sb->list, &ht->tab[h]);
	ht->collisions++;
	ht->nr++;
	return sb;
}

struct stat_bucket *get_key_stat(struct hash_table *ht, char *key, int key_len)
{
	unsigned h = ht->hash_fn(key, key_len) & ht->table_mask;
	struct stat_bucket *sb;

	if (list_empty(&ht->tab[h]))
		return NULL;
	list_for_each_entry(sb, &ht->tab[h], list) {
		if (!memcmp(key, sb->key, key_len))
			return sb;
	}
	return NULL;
}

int stat_bucket_cmp(st_list *l1, st_list *l2)
{
	struct stat_bucket *b1, *b2;

	b1 = container_of(l1, struct stat_bucket, list);
	b2 = container_of(l2, struct stat_bucket, list);
	return b1->count - b2->count;
}

void sort_stat_table(struct hash_table *ht, st_list *head)
{
	unsigned i;
	struct stat_bucket *sb;

	for (i = 0; i < ht->max_nr; i++)
		list_join(&ht->tab[i], head);
	list_sort(head, &stat_bucket_cmp);
}

int main(int argc, char **argv)
{
	struct hash_table ht;
	struct st_bucket *b;
	struct stat_bucket *sb;
	char buffer[64];
	st_list head;
	int i;

	init_list(&head);
	srand(time(NULL));
	alloc_hash_tab(&ht, 1000, &fnv_hash);
	for (i = 0; i < 5000; i++) {
		sprintf(buffer, "KEY#%d", rand()%20);
		increase_key_stat(&ht, buffer, strlen(buffer));
	}
	printf("%d elements inserted, %d collisions\n", ht.nr, ht.collisions);
	b = find_key(&ht, "KEY 4", 5);
	/*
	hlist_for_each_entry(sb, &ht, i) {
		printf("KEY: %s count:%lu\n", (char *)sb->key, sb->count);
		free_stat_bucket(sb);
	} */
	sort_stat_table(&ht, &head);
	list_for_each_entry(sb, &head, list)
		printf("KEY: %s count:%lu\n", (char *)sb->key, sb->count);
}
