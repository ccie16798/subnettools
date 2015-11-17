/*
 *  IPv4, IPv6 functions (compare, conversion to/from string, aggregate etc...)
 *
 * Copyright (C) 2014 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "st_options.h"
#include "debug.h"
#include "iptools.h"
#include "utils.h"
#include "bitmap.h"
#include "heap.h"
#include "st_memory.h"
#include "st_printf.h"

sprint_hex(short)
sprint_unsigned(int)


int is_equal_ip(struct ip_addr *ip1, struct ip_addr *ip2)
{
	if (ip1->ip_ver != ip2->ip_ver)
		return 0;
	if (ip1->ip_ver == IPV4_A && ip1->ip == ip2->ip)
		return 1;
	if (ip1->ip_ver == IPV6_A && is_equal_ipv6(ip1->ip6, ip2->ip6))
		return 1;
	return 0;
}

int ipv6_is_link_local(ipv6 a)
{
	unsigned short x = block(a, 0);
/* link_local address is FE80::/10 */
	return ((x >> 6) == (0xFE80 >> 6));
}

int ipv6_is_global(ipv6 a)
{
	unsigned short x = block(a, 0);
/* global address is 2000::/3 */
	return ((x >> 13) == (0x2000 >> 13));
}

int ipv6_is_ula(ipv6 a)
{
	unsigned short x = block(a, 0);
/* ula address is FC00::/7 */
	return ((x >> 9)  == (0xFC00 >> 9));
}

int ipv6_is_multicast(ipv6 a)
{
	/* IPv6 Multicast is FF00/8 */
	return ((block(a, 0) >> 8) == (0xFF00 >> 8));
}

static inline int subnet_compare_ipv6(ipv6 ip1, int mask1, ipv6 ip2, int mask2)
{
	if (mask1 > mask2) {
		shift_ipv6_right(ip1, (128 - mask2));
		shift_ipv6_right(ip2, (128 - mask2));
		if (is_equal_ipv6(ip1, ip2))
			return INCLUDED;
		else
			return NOMATCH;
	} else if (mask1 < mask2) {
		shift_ipv6_right(ip1, (128 - mask1));
		shift_ipv6_right(ip2, (128 - mask1));
		if (is_equal_ipv6(ip1, ip2))
			return INCLUDES;
		else
			return NOMATCH;
	} else {
		shift_ipv6_right(ip1, (128 - mask1));
		shift_ipv6_right(ip2, (128 - mask1));
		if (is_equal_ipv6(ip1, ip2))
			return EQUALS;
		else
			return NOMATCH;
	}
}

static inline int subnet_compare_ipv4(ipv4 prefix1, int mask1, ipv4 prefix2, int mask2)
{
	ipv4 a, b;

	if (mask1 > mask2) {
		a = prefix1 >> (32 - mask2);
		b = prefix2 >> (32 - mask2);
		if (a == b)
			return INCLUDED;
		else
			return NOMATCH;
	} else if  (mask1 < mask2) {
		a = prefix1 >> (32 - mask1);
		b = prefix2 >> (32 - mask1);
		if (a == b)
			return INCLUDES;
		else
			return NOMATCH;
	} else {
		a = prefix1 >> (32 - mask1);
		b = prefix2 >> (32 - mask1);
		if (a == b)
			return EQUALS;
		else
			return NOMATCH;
	}
}

int subnet_compare(const struct subnet *sub1, const struct subnet *sub2)
{
	if (sub1->ip_ver != sub2->ip_ver) {
		debug(ADDRCOMP, 1, "different address FAMILY : %d, %d\n",
				sub1->ip_ver, sub2->ip_ver);
		return -1;
	}
	if (sub1->ip_ver == IPV4_A)
		return subnet_compare_ipv4(sub1->ip, sub1->mask, sub2->ip, sub2->mask);
	else if (sub1->ip_ver == IPV6_A)
		return subnet_compare_ipv6(sub1->ip6, sub1->mask, sub2->ip6, sub2->mask);
	fprintf(stderr, "Impossible to get here, IP version = %d BUG?\n", sub1->ip_ver);
	return -1;
}

int addr_compare(const struct ip_addr *a, const struct subnet *sub)
{
	if (a->ip_ver != sub->ip_ver) {
		debug(ADDRCOMP, 1, "different address FAMILY : %d, %d\n", a->ip_ver, sub->ip_ver);
		return -1;
	}
	if (a->ip_ver == IPV4_A)
		return subnet_compare_ipv4(a->ip, 32, sub->ip, sub->mask);
	else if (a->ip_ver == IPV6_A)
		return subnet_compare_ipv6(a->ip6, 128, sub->ip6, sub->mask);
	fprintf(stderr, "Impossible to get here, IP version = %d BUG?\n", a->ip_ver);
	return -1;
}

static inline int addrv42str(ipv4 z, char *out_buffer, size_t len)
{
	int i;
	/*
	 * instead of using snprint to check outbuff isnt overrun,
	 * we make sure output buffer is large enough
	 * we refuse to print potentially truncated IPs and BUG early ; min size is (3 + 1) * 4
	 */
	if (len < 16) {
		fprintf(stderr, "BUG, %s needs at least a 16-bytes buffer\n", __func__);
		out_buffer[0] = '\0';
		return -1;
	}
	i = sprint_uint(out_buffer, (z >> 24) & 0xff);
	out_buffer[i++] = '.';
	i += sprint_uint(out_buffer + i, (z >> 16) & 0xff);
	out_buffer[i++] = '.';
	i += sprint_uint(out_buffer + i, (z >> 8) & 0xff);
	out_buffer[i++] = '.';
	i += sprint_uint(out_buffer + i, z & 0xff);
	out_buffer[i] = '\0';
	return i;
}

/*
 * store human readable of IPv6 z in output
 * output MUST be large enough
 * compress = 0 ==> no adress compression
 * compress = 1 ==> remove leading zeros
 * compress = 2 ==> FULL compression but doesnt convert Embedded IPv4
 * compress = 3 ==> FULL compression and convert Embedded IPv4
 */
static inline int addrv62str(ipv6 z, char *out_buffer, size_t len, int compress)
{
	int a, i, j;
	int skip = 0, max_skip = 0;
	int skip_index = 0, max_skip_index = 0;

	/*
	 * instead of using snprint to check outbuff isnt overrun at each step,
	 * we make sure output buffer is large enough
	 * we refuse to print potentially truncated IPs and BUG early ; min size if (4 + 1) * 8
	 */
	if (len < 40) {
		fprintf(stderr, "BUG, %s needs at least a 40-bytes buffer\n", __func__);
		out_buffer[0] = '\0';
		return -1;
	}
	if (compress == 0) {
		/* no need for snprintf since we made sure len is at least 40 and
		 * we can't print more than 40 chars here
		 */
		a = sprintf(out_buffer, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
				block(z, 0), block(z, 1), block(z, 2), block(z, 3),
				block(z, 4), block(z, 5), block(z, 6), block(z, 7));
		return a;
	} else if (compress == 1) {
		a = sprintf(out_buffer, "%x:%x:%x:%x:%x:%x:%x:%x",
				block(z, 0), block(z, 1), block(z, 2), block(z, 3),
				block(z, 4), block(z, 5), block(z, 6), block(z, 7));
		return a;
	}
	/* longest 0000 block sequence will be replaced */
	for (i = 0; i < 8; i++) {
		if (block(z, i) == 0) {
			if (skip == 0)  {
				debug(PARSEIPV6, 8, "possible skip index %d\n", i);
				skip_index = i;
			}
			skip++;
		} else {
			if (skip) {
				/* in case of egality, we prefer to replace block to the right
				 * if you want to replace the left block change to '>'
				 */
				if (skip >= max_skip) {
					debug(PARSEIPV6, 8, "skip index %d better\n", skip_index);
					max_skip = skip;
					max_skip_index = skip_index;
				}
				skip = 0;
			}
		}
	}
	if (skip && (skip >= max_skip)) {
		/* happens in case address ENDS with :0000,
		 * we then left the loop without setting max_skip
		 */
		max_skip =  skip;
		max_skip_index = skip_index;
	}
	if (max_skip == 1) /* do not bother to compress if there is only one block to compress */
		max_skip = max_skip_index = 0;
	debug(PARSEIPV6, 5, "can skip %d blocks at index %d\n", max_skip, max_skip_index);
	if (compress == 3 && (skip_index == 0 && (max_skip >= 5 && max_skip < 8))) {
		/* Mapped & Compatible IPv4 address */
		if (block(z, 5) == 0x0) {
			if (block(z, 6) == 0 && block(z, 7) == 1) /** the loopback address */
				return sprintf(out_buffer, "::1");
			else
				return sprintf(out_buffer, "::%d.%d.%d.%d",
						block(z, 6) >> 8, block(z, 6) & 0xff,
						block(z, 7) >> 8, block(z, 7) & 0xff);
		}
		if (block(z, 5) == 0xffff)
			return sprintf(out_buffer, "::ffff:%d.%d.%d.%d",
					block(z, 6) >> 8, block(z, 6) & 0xff,
					block(z, 7) >> 8, block(z, 7) & 0xff);
	}
	j = 0;
	for (i = 0; i < max_skip_index; i++) {
		j += sprint_hexshort(out_buffer + j, block(z, i));
		out_buffer[j++] = ':';
	}
	if (max_skip > 0) {
		out_buffer[j++] = ':';
		if (!max_skip_index) /* means addr starts with 0 */
			out_buffer[j++] = ':';
	}
	for (i = max_skip_index + max_skip; i < 7; i++) {
		j += sprint_hexshort(out_buffer + j, block(z, i));
		out_buffer[j++] = ':';
	}
	if (i < 8)
		j += sprint_hexshort(out_buffer + j, block(z, i));
	out_buffer[j] = '\0';
	return j;
}

static inline int addrv42bitmask(ipv4 a, char *out, size_t len)
{
	int i;

	if (len < 33) {
		fprintf(stderr, "BUG, %s needs at least a 33-bytes buffer\n", __func__);
		out[0] = '\0';
		return -1;
	}
	for (i = 0; i < 32; i++)
		out[i] = '0' + !!(a & (1  << (31 - i)));
	out[i] = '\0';
	return i;
}

static inline int addrv62bitmask(ipv6 a, char *out, size_t len)
{
	int i;

	if (len < 129) {
		fprintf(stderr, "BUG, %s needs at least a 129-bytes buffer\n", __func__);
		out[0] = '\0';
		return -1;
	}
	for (i = 0; i < 128; i++)
		out[i] = '0' + !!(block(a, i / 16) & (1 << ((127 - i) & 0xf)));
	out[i] = '\0';
	return i;
}

int addr2bitmask(const struct ip_addr *a, char *out, size_t len)
{
	if (a->ip_ver == IPV4_A)
		return addrv42bitmask(a->ip, out, len);
	if (a->ip_ver == IPV6_A)
		return addrv62bitmask(a->ip6, out, len);
	out[0] = '\0';
	return -1;
}

/* outbuffer must be large enough **/
int subnet2str(const struct subnet *s, char *out_buffer, size_t len, int comp_level)
{
	if (comp_level == 4)
		return addr2bitmask(&s->ip_addr, out_buffer, len);
	if (s->ip_ver == IPV4_A)
		return addrv42str(s->ip, out_buffer, len);
	if (s->ip_ver == IPV6_A)
		return addrv62str(s->ip6, out_buffer, len, comp_level);
	out_buffer[0] = '\0';
	return -1;
}

/* outbuffer must be large enough **/
int addr2str(const struct ip_addr *a, char *out_buffer, size_t len, int comp_level)
{
	if (comp_level == 4)
		return addr2bitmask(a, out_buffer, len);
	if (a->ip_ver == IPV4_A)
		return addrv42str(a->ip, out_buffer, len);
	if (a->ip_ver == IPV6_A)
		return addrv62str(a->ip6, out_buffer, len, comp_level);
	out_buffer[0] = '\0';
	return -1;
}
/* IPv4 only, mask to Dot Decimal Notation */
int mask2ddn(int mask, char *out, size_t len)
{
	int i;
	unsigned int s[4];

	if (len < 16) {
		fprintf(stderr, "BUG, %s needs at least a 16-bytes buffer\n", __func__);
		out[0] = '\0';
		return -1;
	}
	for (i = 0; i < mask / 8; i++)
		s[i] = 255;
	if (i < 4) {
		s[i++] = 256 - (1 << (8 - (mask % 8)));
		for ( ; i < 4; i++)
			s[i] = 0;
	}
	return sprintf(out, "%d.%d.%d.%d", s[0], s[1], s[2], s[3]);
}

int ipv4_get_classfull_mask(const struct subnet *s)
{
	if (s->ip == 0)
		return 0;
	if ((s->ip >> 31) == 0)
		return 8;
	if ((s->ip >> 30) == 2)
		return 16;
	if ((s->ip >> 29) == 6)
		return 24;
	if ((s->ip >> 28) == 14) /* MULTICAST is /32 but hum ... */
		return 32;
	if ((s->ip >> 28) == 15)
		return -1;
	return -1;
}

int addr_is_superior(const struct ip_addr *ip1, const struct ip_addr *ip2)
{
	int i, res;

	if (ip1->ip_ver != ip2->ip_ver) {
		debug(ADDRCOMP, 1, "cannot compare, different IP version %d != %d\n",
				ip1->ip_ver, ip2->ip_ver);
		return -1;
	}
	res = 0;
	if (ip1->ip_ver == IPV4_A)
		return  (ip1->ip < ip2->ip);

	if (ip1->ip_ver == IPV6_A) {
		for (i = 0; i < 8; i++) {
			if (block(ip1->ip6, i) < block(ip2->ip6, i)) {
				res = 1;
				break;
			} else if (block(ip1->ip6, i) > block(ip2->ip6, i)) {
				res = 0;
				break;
			}
		}
		return res;
	}
	return -1;
}

int subnet_is_superior(const struct subnet *s1, const struct subnet *s2)
{
	int i, res;

	if (s1->ip_ver != s2->ip_ver) {
		debug(ADDRCOMP, 1, "cannot compare, different IP version\n");
		return -1;
	}
	res = 0;
	if (s1->ip_ver == IPV4_A) {
		if (s1->ip == s2->ip) {
			if (s1->mask < s2->mask)
				res = 1;
			else
				res = 0;
		} else if  (s1->ip < s2->ip)
			res =  1;
		else
			res =  0;
		st_debug(ADDRCOMP, 7, "%P %c %P\n", *s1, (res ? '>' : '<'), *s2);
		return res;
	}

	if (s1->ip_ver == IPV6_A) {
		if (is_equal_ipv6(s1->ip6, s2->ip6)) {
			if (s1->mask < s2->mask)
				res = 1;
			else
				res = 0;
		} else {
			for (i = 0; i < 8; i++) {
				if (block(s1->ip6, i) < block(s2->ip6, i)) {
					res = 1;
					break;
				} else if (block(s1->ip6, i) > block(s2->ip6, i)) {
					res = 0;
					break;
				}
			}
		}
		st_debug(ADDRCOMP, 7, "%P %c %P\n", *s1, (res ? '>' : '<'), *s2);
		return res;
	}
	debug(ADDRCOMP, 1, "Invalid comparison ipver %d\n", s1->ip_ver);
	return -1;
}

int subnet_filter(const struct subnet *test, const struct subnet *against, char op)
{
	int res;

	res = subnet_compare(test, against);
	switch (op) {
	case '=':
		return (res == EQUALS);
	case '#':
		return !(res == EQUALS);
	case '<':
		return subnet_is_superior(test, against);
	case '>':
		return !subnet_is_superior(test, against) && res != EQUALS;
	case '{':
		return (res == INCLUDED || res == EQUALS);
	case '}':
		return (res == INCLUDES || res == EQUALS);
	default:
		debug(FILTER, 1, "Unsupported op '%c' for subnet_filter\n", op);
		return -1;
	}
}

int addr_filter(const struct ip_addr *test, const struct subnet *against, char op)
{
	int res;

	res = addr_compare(test, against);
	switch (op) {
	case '=':
		return (res == EQUALS);
	case '#':
		return !(res == EQUALS);
	case '<':
		return addr_is_superior(test, &against->ip_addr);
	case '>':
		return !addr_is_superior(test, &against->ip_addr) && res != EQUALS;
	case '{':
		return (res == INCLUDED || res == EQUALS);
	case '}':
		return (res == INCLUDES || res == EQUALS);
	default:
		debug(FILTER, 1, "Unsupported op '%c' for addr_filter\n", op);
		return -1;
	}
}

/* try to aggregate s1 & s2, putting the result in res if possible
 * returns negative if impossible to aggregate, positive if possible
 */
static int aggregate_subnet_ipv4(const struct subnet *s1, const struct subnet *s2,
		struct subnet *res)
{
	ipv4 a, b;

	if (s1->mask != s2->mask) {
		st_debug(AGGREGATE, 5, "different masks for %P and %P, can't aggregate\n",
				*s1, *s2);
		return -1;
	}
	a = s1->ip >> (32 - s1->mask);
	b = s2->ip >> (32 - s1->mask);
	if (a == b) {
		st_debug(AGGREGATE, 6, "same subnet %P\n", *s1);
		res->ip_ver = IPV4_A;
		res->ip     = s1->ip;
		res->mask   = s1->mask;
		return 1;
	}
	if (a >> 1 != b >> 1) {
		st_debug(AGGREGATE, 5, "cannot aggregate %P and %P\n", *s1, *s2);
		return -1;
	}
	res->ip_ver = IPV4_A;
	res->mask = s1->mask - 1;
	res->ip   = (a >> 1) << (32 - res->mask);
	st_debug(AGGREGATE, 5, "can aggregate %P and %P into : %P\n", *s1, *s2, *res);
	return 1;
}

static int aggregate_subnet_ipv6(const struct subnet *s1, const struct subnet *s2,
		struct subnet *res)
{
	ipv6 a, b;

	if (s1->mask != s2->mask) {
		st_debug(AGGREGATE, 5, "different masks for %P and %P, can't aggregate\n",
				*s1, *s2);
		return -1;
	}
	memcpy(&a, &s1->ip6, sizeof(a));
	memcpy(&b, &s2->ip6, sizeof(a));
	shift_ipv6_right(a, (128 - s1->mask));
	shift_ipv6_right(b, (128 - s1->mask));
	if (is_equal_ipv6(a, b)) {
		st_debug(AGGREGATE, 5, "same subnet %P\n", *s1);
		copy_subnet(res, s1);
		return 1;
	}
	shift_ipv6_right(a, 1);
	shift_ipv6_right(b, 1);
	if (!is_equal_ipv6(a, b)) {
		st_debug(AGGREGATE, 5, "cannot aggregate %P and %P\n", *s1, *s2);
		return -1;
	}
	res->mask = s1->mask - 1;
	shift_ipv6_left(a, 128 - res->mask);
	res->ip_ver = IPV6_A;
	memcpy(&res->ip6, &a, sizeof(a));
	st_debug(AGGREGATE, 5, "can aggregate %P and %P into : %P\n", *s1, *s2, *res);
	return 1;
}

int aggregate_subnet(const struct subnet *s1, const struct subnet *s2, struct subnet *res)
{
	if (s1->ip_ver != s2->ip_ver)
		return -4;

	if (s1->ip_ver == IPV4_A)
		return aggregate_subnet_ipv4(s1, s2, res);
	if (s1->ip_ver == IPV6_A)
		return aggregate_subnet_ipv6(s1, s2, res);
	return -3;
}

/* network address of a prefix; named first_ip becoz in IPv6 ...
 * well network address is a regular address
 */
void first_ip(struct subnet *s)
{
	if (s->ip_ver == IPV4_A) {
		s->ip >>= (32 - s->mask);
		s->ip <<= (32 - s->mask);
	} else if (s->ip_ver == IPV6_A) {
		shift_ipv6_right(s->ip6, 128 - s->mask);
		shift_ipv6_left(s->ip6, 128 - s->mask);
	}
}

/* broadcast address of a prefix; except broadcast doesnt exist in IPv6 :) */
void last_ip(struct subnet *s)
{
	int i, j;

	if (s->ip_ver == IPV4_A) {
		for (i = 0; i < (32 - s->mask); i++)
			s->ip |= (1 << i);
	} else if (s->ip_ver == IPV6_A) { /* do you love binary math? yes you do! */
		for (i = 0; i < (128 - s->mask) / 16; i++)
			set_block(s->ip6, 7 - i, 0xFFFF);
		for (j = 0; j < ((128 - s->mask) % 16); j++)
			block_OR(s->ip6, 7 - i,  (1 << j));
	}
}

void next_subnet(struct subnet *s)
{
	if (s->ip_ver == IPV4_A) {
		s->ip >>= (32 - s->mask);
		s->ip += 1;
		s->ip <<= (32 - s->mask);
	} else if (s->ip_ver == IPV6_A) {
		shift_ipv6_right(s->ip6, 128 - s->mask);
		increase_ipv6(s->ip6);
		shift_ipv6_left(s->ip6, 128 - s->mask);
	}
}

void previous_subnet(struct subnet *s)
{
	if (s->ip_ver == IPV4_A) {
		s->ip >>= (32 - s->mask);
		s->ip -= 1;
		s->ip <<= (32 - s->mask);
	} else if (s->ip_ver == IPV6_A) {
		shift_ipv6_right(s->ip6, 128 - s->mask);
		decrease_ipv6(s->ip6);
		shift_ipv6_left(s->ip6, 128 - s->mask);
	}
}

int can_decrease_mask(const struct subnet *s)
{
	ipv4 a;
	ipv6 b;
	int i = 0;

	if (s->ip_ver == IPV4_A) {
		a = (s->ip >> (32 - s->mask));
		while (i < s->mask) {
			if (a & 1)
				break;
			i++;
			a >>= 1;
		}
		return i;
	}
	if (s->ip_ver == IPV6_A) {
		memcpy(&b, &s->ip6, sizeof(ipv6));
		shift_ipv6_right(b, 128 - s->mask);
		while (i < s->mask) {
			if (block(b, 7) & 1)
				break;
			i++;
			shift_ipv6_right(b, 1);
		}
		return i;
	}
	return 0;
}

/*
 * remove s2 from s1 if possible
 * alloc a new struct subnet *
 * number of element is stored in *n
 */
struct subnet *subnet_remove(const struct subnet *s1, const struct subnet *s2, int *n)
{
	int res;
	struct subnet *news;
	struct subnet test;
	int a, i;

	res = subnet_compare(s1, s2);
	if (res == EQUALS) {
		st_debug(ADDRREMOVE, 5, "same prefix %P\n", *s1);
		*n = 0;
		return NULL;
	} else if (res == NOMATCH || res == INCLUDED) {
		news = st_malloc_nodebug(sizeof(struct subnet), "subnet");
		if (news == NULL) {
			*n = -1;
			return NULL;
		}
		st_debug(ADDRREMOVE, 5, "%P is not included in %P\n", *s2, *s1);
		copy_subnet(news, s1);
		*n = 1;
		return news;
	}
	/* s2 in included in s1 */
	res = s2->mask - s1->mask;
	news = st_malloc_nodebug(sizeof(struct subnet) * res, "subnet");
	if (news == NULL) {
		*n = -1;
		return NULL;
	}
	copy_subnet(&test, s1);
	first_ip(&test);
	i = 0;
	/* strategy is as follow :
	* test = S1;
	* do
	*	do
	*		increase test mask
	*	until test doesn't include S2 any more
	*
	*	test = test + 1 (next subnet)
	* until test != S2
	*
	* test = S2
	* do
	*	test = test +1 (next subnet)
	*	decrease test mask as much as we can (aggregation)
	*until test included in S1
	*/
	while (1) { /* getting subnet before s2*/
		a = 0;
		while (1) {
			test.mask += 1;
			st_debug(ADDRREMOVE, 3, "Loop#1 testing %P\n", test);
			res = subnet_compare(&test, s2);
			if (res == INCLUDES) {
				st_debug(ADDRREMOVE, 5,
						"Loop#1 %P too BIG, includes %P, continuing loop\n",
						test, *s2);
				continue;
			} else if (res == NOMATCH) {
				st_debug(ADDRREMOVE, 5,
						"Loop#1 %P BIG enough, breaking loop\n", test);
				copy_subnet(&news[i], &test);
				i++;
				break;
			} else if (res == EQUALS) {
				st_debug(ADDRREMOVE, 5,
						"Loop#1 %P EQUALS, breaking final loop\n", test);
				a++;
				break;
			}
		}
		if (a)
			break;
		next_subnet(&test);
		st_debug(ADDRREMOVE, 5, "Loop#1 advancing to %P\n", test);
		res = subnet_compare(&test, s2);
		if (res == EQUALS) {
			st_debug(ADDRREMOVE, 5, "Loop#1 finally we reached %P\n", *s2);
			break;
		}
	}
	/* getting subnet after s2 */
	copy_subnet(&test, s2);

	while (1) {
		next_subnet(&test);
		st_debug(ADDRREMOVE, 5, "Loop#2 testing %P\n", test);
		a = can_decrease_mask(&test);
		test.mask -= a;
		st_debug(ADDRREMOVE, 5, "Loop#2 increased to %P\n", test);
		res = subnet_compare(&test, s1);
		if (res == NOMATCH) {
			st_debug(ADDRREMOVE, 5, "Loop#2 finally %P bigger than %P\n", test, *s1);
			break;
		}
		copy_subnet(&news[i], &test);
		i++;
	}
	*n = i;
	return news;
}
