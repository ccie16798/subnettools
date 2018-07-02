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
	unsigned int max_nr; /* power of two */
	unsigned int table_mask; /* table_size - 1, used to do MODULUS */
	int collisions;
	unsigned int nr;
	st_list *tab;
	unsigned (*hash_fn)(void *, int);
};

/* two well-known hash-functions, see .c file for authors & copyrights */
unsigned djb_hash(void *key, int len);
unsigned fnv_hash(void *key, int len);
/* hlist_for_each_entry: iterate over all elements in a hash_table
 * @elem   : the iterator (the list embedded inside must be named 'list')
 * @__ht   : the hash table
 * @__i    : an unsigned to iterate over the table
 */
#define hlist_for_each_entry(elem, __ht, __i) \
	for (__i = 0; __i < (__ht)->max_nr; __i++)  \
		if (!list_empty(&(__ht)->tab[__i])) \
			list_for_each_entry(elem, &(__ht)->tab[__i], list)

/* alloc_hash_tab: setup a new hash table with nr elements
 * @ht   : an existing hash table
 * @nr   : the size of the hash table (will be rounded to next power of 2)
 * @hash : a hash function (djb, fnv...)
 * returns:
 *	>0 on SUCCESS
 *	<0 on ENOMEM
 */
int alloc_hash_tab(struct hash_table *ht, unsigned long nr, unsigned (*hash)(void *, int));

/* new_stat_bucket: allocate memory for a stat bucket
 * @key     : the key
 * @key_len : the length of the key
 * returns:
 *	pointer to a new struct on SUCCESS
 *	NULL on ENOMEM
 */
struct stat_bucket *new_stat_bucket(char *key, int key_len);

/* free_stat_bucket: free a stat bucket, remove from list
 * @sb   : a pointer to a stat bucket
 */
void free_stat_bucket(struct stat_bucket *sb);

/* free_stat_hash_table: free all bucket in hash table and hash_table itself
 * @ht   : a pointer to a hash_table
 */
void free_stat_hash_table(struct hash_table *ht);

/* insert_bucket: insert a bucket in a hash_table
 * @ht     : the hash table
 * @bucket : the bucket to insert
 */
void insert_bucket(struct hash_table *ht, struct st_bucket *bucket);


/* find_key: find a bucket by key
 * @ht      : the hash table
 * @key     : the key to look
 * @key_len : its length
 * returns:
 *	a pointer to a struct bucket if found
 *	NULL if not found
 */
struct st_bucket *find_key(struct hash_table *ht, char *key, int key_len);


/* remove_key: remove a bucket from a hash table (without freeing it)
 * @ht      : the hash table
 * @key     : the key to look
 * @key_len : its length
 * returns:
 *	a pointer to a struct bucket if found
 *	NULL if not found
 */
struct st_bucket *remove_key(struct hash_table *ht, char *key, int key_len);


/* increase_key_stat:  increase the count associated with a key; alloc memory on first count
 * @ht      : the hash table
 * @key     : the key to look
 * @key_len : its length
 * returns:
 *	a pointer to the key_stat if successful
 *	NULL on ENOMEM
 */
struct stat_bucket *increase_key_stat(struct hash_table *ht, char *key, int key_len);

/* get_key_stat:  get the struct bucket_stat associated with a key
 * @ht      : the hash table
 * @key     : the key to look
 * @key_len : its length
 * returns:
 *	a pointer to the bucket_stat if found
 *	NULL if not found
 */
struct stat_bucket *get_key_stat(struct hash_table *ht, char *key, int key_len);
#else
#endif
