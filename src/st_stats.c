/*
 * stats -- code to create statistics about subnet_files, ipam_files, ..
 *
 * Copyright (C) 2018 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "iptools.h"
#include "st_list.h"
#include "st_hashtab.h"
#include "st_scanf.h"
#include "st_options.h"
#include "st_stats.h"
#include "st_object.h"
#include "st_printf.h"


unsigned int hash_ipaddr(const void *key, int len)
{
	const struct ip_addr *a = key;

	if (a->ip_ver == IPV4_A)
		return djb_hash(&a->ip, 4);
	if (a->ip_ver == IPV6_A)
		return djb_hash(&a->ip6, 16);
	fprintf(stderr, "cannot hash unknown ip version %d\n", a->ip_ver);
	return 0;
}

unsigned int hash_subnet(const void *key, int len)
{
	const struct subnet *a = key;

	if (a->ip_addr.ip_ver == IPV4_A || a->ip_addr.ip_ver == IPV4_N)
		return djb_hash(&a->ip_addr.ip, 4) * a->mask;
	if (a->ip_addr.ip_ver == IPV6_A || a->ip_addr.ip_ver == IPV6_N)
		return djb_hash(&a->ip_addr.ip6, 16) * a->mask;
	fprintf(stderr, "cannot hash unknown ip version %d\n", a->ip_ver);
	return 0;
}

int ipam_stats(struct ipam_file *ipam, const char *statvalue)
{
	int res, ea_index = -1;
	unsigned long i;
	struct hash_table ht;
	struct stat_bucket *sb;
	struct st_ea *ea;
	struct subnet *subnet;
	st_list head;

	if (!strcmp(statvalue, "subnet")) {
		res = alloc_hash_tab(&ht, ipam->nr, &hash_subnet);
		if (res < 0)
			return res;

		for (i = 0; i < ipam->nr; i++) {
			subnet = &ipam->lines[i].subnet;
			increase_key_stat(&ht, (char *)subnet, sizeof(*subnet));
		}
	} else {
		res = alloc_hash_tab(&ht, ipam->nr, &djb_hash);
		for (i = 0; i < ipam->ea_nr; i++) {
			if (!strcmp(statvalue, ipam->ea[i]))
				ea_index = i;
		}
		if (ea_index < 0) {
			fprintf(stderr, "unknown EA '%s'\n", statvalue);
			return -1;
		}
		for (i = 0; i < ipam->nr; i++) {
			ea = &ipam->lines[i].ea[ea_index];
			if (ea->value)
				increase_key_stat(&ht, ea->value, ea->len);
		}
	}
	init_list(&head);
	sort_stat_table(&ht, &head);

	if (strcmp(statvalue, "subnet")) {
		list_for_each_entry(sb, &head, list) {
			printf("%s,%s,%lu\n",
					statvalue, (char *)sb->key, sb->count);
			free_stat_bucket(sb);
		}
	} else {
		list_for_each_entry(sb, &head, list) {
			st_printf("%s,%P,%lu\n",
					statvalue, *((struct subnet *)sb->key), sb->count);
			free_stat_bucket(sb);
		}
	}
	/* we cannot use free_stat_hash_tab since the call to sort_stat_table modified
	 * the structure of the hash table
	 */
	st_free(ht.tab, ht.max_nr * sizeof(struct st_list));
	return 1;
}
