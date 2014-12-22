/*
 * IP address information
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
#include "iptools.h"
#include "debug.h"
#include "st_printf.h"
#include "ipinfo.h"

const struct subnet class_a = { .ip_ver = IPV4_A, .ip = 0, .mask = 1};
const struct subnet class_b = { .ip_ver = IPV4_A, .ip = (2 << 30), .mask = 2};
const struct subnet class_c = { .ip_ver = IPV4_A, .ip = (6 << 29), .mask = 3};
const struct subnet class_d = { .ip_ver = IPV4_A, .ip = (14 << 28), .mask = 4};
const struct subnet class_e = { .ip_ver = IPV4_A, .ip = (15 << 28), .mask = 4};

#define S_IPV4_CONST(DIGIT1, DIGIT2, __MASK)  { .ip_ver = IPV4_A, .ip = (DIGIT1 << 24) + (DIGIT2 << 16), .mask = __MASK }
const struct subnet ipv4_broadcast	= { .ip_ver = IPV4_A, .ip = -1, .mask = 32};
const struct subnet ipv4_default	= S_IPV4_CONST(0,  0,   0);
const struct subnet ipv4_unspecified	= S_IPV4_CONST(0,  0,   32);
const struct subnet ipv4_rfc1918_1	= S_IPV4_CONST(10,  0,   8);
const struct subnet ipv4_rfc1918_2	= S_IPV4_CONST(172, 16,  12);
const struct subnet ipv4_rfc1918_3	= S_IPV4_CONST(192, 168, 16);
const struct subnet ipv4_loopback	= S_IPV4_CONST(127, 0,   8);
const struct subnet ipv4_rfc3927_ll	= S_IPV4_CONST(169, 254, 16);
const struct subnet ipv4_rfc6598_sasr	= S_IPV4_CONST(100, 64,  10);
const struct subnet ipv4_bench		= S_IPV4_CONST(198, 18,  15);
#define S_IPV4_CONST3(DIGIT1, DIGIT2, DIGIT3,__MASK)  { .ip_ver = 4, .ip = (DIGIT1 << 24) + (DIGIT2 << 16) + (DIGIT3 << 8), .mask = __MASK }
const struct subnet ipv4_test1		= S_IPV4_CONST3(192, 0, 2, 24);
const struct subnet ipv4_test2		= S_IPV4_CONST3(198, 51, 100, 24);
const struct subnet ipv4_test3		= S_IPV4_CONST3(203, 0, 113, 24);

#define S_IPV6_CONST(DIGIT1, DIGIT2, __MASK)  { .ip_ver = 6, .ip6.n16[0] = DIGIT1, .ip6.n16[1] = DIGIT2, .mask = __MASK }
const struct subnet ipv6_default	= S_IPV6_CONST(0x0000, 0, 0);
const struct subnet ipv6_unspecified	= S_IPV6_CONST(0x0000, 0, 128);
const struct subnet ipv6_global		= S_IPV6_CONST(0x2000, 0, 3);
const struct subnet ipv6_ula		= S_IPV6_CONST(0xfc00, 0, 7);
const struct subnet ipv6_sitelocal	= S_IPV6_CONST(0xfec0, 0, 10);
const struct subnet ipv6_linklocal	= S_IPV6_CONST(0xfe80, 0, 10);
const struct subnet ipv6_multicast	= S_IPV6_CONST(0xff00, 0, 8);
const struct subnet ipv6_6to4		= S_IPV6_CONST(0x2002, 0, 16);
const struct subnet ipv6_rfc4380_teredo = S_IPV6_CONST(0x2001, 0, 32);
const struct subnet ipv6_rfc3849_doc	= S_IPV6_CONST(0x2001, 0x0DB8, 32);
const struct subnet ipv6_rfc6052_pat 	= S_IPV6_CONST(0x0064, 0xff9b, 96);
const struct subnet ipv6_isatap_priv_ll	= {.ip_ver = 6, .ip6.n16[0] = 0xFE80, .ip6.n16[5] = 0x5EFE, .mask = 96};
const struct subnet ipv6_isatap_pub_ll	= {.ip_ver = 6, .ip6.n16[0] = 0xFE80, .ip6.n16[4] = 0x0200, .ip6.n16[5] = 0x5EFE, .mask = 96};
const struct subnet ipv6_mapped_ipv4	= {.ip_ver = 6, .ip6.n16[5] = 0xFFFF, .mask = 96}; /* ::FFFF:/96 */
const struct subnet ipv6_compat_ipv4	= {.ip_ver = 6, .mask = 96}; /* ::/96 */
const struct subnet ipv6_loopback 	= {.ip_ver = 6, .ip6.n16[7] = 1, .mask = 128}; /* ::1/128 */

static void decode_6to4(FILE *out, const struct subnet *s) {
	fprintf(out, "6to4 IPv4 destination address : %d.%d.%d.%d\n", s->ip6.n16[1] >> 8, s->ip6.n16[1] & 0xFF,
			s->ip6.n16[2] >> 8, s->ip6.n16[2] & 0xFF);
}

static void decode_teredo(FILE *out, const struct subnet *s) {
	fprintf(out, "Teredo server : %d.%d.%d.%d\n", s->ip6.n16[2] >> 8, s->ip6.n16[2] & 0xFF,
			s->ip6.n16[3] >> 8, s->ip6.n16[3] & 0xFF);
	fprintf(out, "Client IP     : %d.%d.%d.%d\n", (s->ip6.n16[6] >> 8) ^ 0xFF , (s->ip6.n16[6] & 0xFF) ^ 0xFF,
			(s->ip6.n16[7] >> 8) ^ 0xFF, (s->ip6.n16[7] & 0xFF) ^ 0xFF);

	fprintf(out, "UDP port      : %d\n", s->ip6.n16[5] ^ 0xFFFF);
}

static void decode_rfc6052(FILE *out, const struct subnet *s) {
	fprintf(out, "IPv4-Embedded IPv6 address : 64:ff9b::%d.%d.%d.%d\n", s->ip6.n16[6] >> 8, s->ip6.n16[6] & 0xFF,
			s->ip6.n16[7] >> 8, s->ip6.n16[7] & 0xFF);
}

static void decode_isatap_ll(FILE *out, const struct subnet *s) {
	fprintf(out, "ISATAP IPv4 destination address : %d.%d.%d.%d\n", s->ip6.n16[6] >> 8, s->ip6.n16[6] & 0xFF,
			s->ip6.n16[7] >> 8, s->ip6.n16[7] & 0xFF);
}


void decode_ipv4_multicast(FILE *out, const struct subnet *s) {
	int i, res;
	struct known_subnet_desc *k = ipv4_mcast_known_subnets;
	int found_mask, found_i;

	found_mask = -1;
	for (i = 0; ;i++) {
		if (k[i].s == NULL)
			break;
		res = subnet_compare(s, k[i].s);
		st_debug(ADDRCOMP, 5, "%P against %P\n", *s, *k[i].s);
		if (res == EQUALS || (res == INCLUDED && k[i].always_print != -1)) {
			if ((int)k[i].s->mask >= found_mask) {
				found_mask = k[i].s->mask;
				found_i = i;
			}
		}


	}
	if (found_mask >= 0) {
		fprintf(out, "%s\n", k[found_i].desc);
		if (k[found_i].decode_more)
			k[found_i].decode_more(out, s);
	}
}

static void decode_ipv6_embedded_rp(FILE *out, const struct subnet *s) {
	struct subnet rp;
	int rp_id;
	int Plen;
	u32 group_id;
	int i, j;

	memset(&rp, 0, sizeof(rp));
	rp.ip_ver = IPV6_A;
	rp.mask   = 128;
	rp_id     = (s->ip6.n16[1] >> 8) & 0xF;
	Plen      = s->ip6.n16[1] & 0xFF;
	if (Plen > 64) {
		fprintf(out, "Invalid Plen %x, MUST not be greater than 64\n", Plen);
		return;
	}
	group_id = s->ip6.n16[6] * (1 << 16) + s->ip6.n16[7];
	/* copying Plen bits of s into rp */
	for (i = 0; i < Plen / 16; i++)
		rp.ip6.n16[i] = s->ip6.n16[i + 2];
	for (j = 0 ; j < Plen % 16; j++)
		rp.ip6.n16[i] |= (s->ip6.n16[i + 2] & (1 << (15 - j)));
	rp.ip6.n16[7] = rp_id;
	st_fprintf(out, "Embedded RP Address : %I\n", rp);
	fprintf(out, "32-bit group id 0x%x [%d]\n", group_id, group_id);
}

static void decode_ipv6_multicast(FILE *out, const struct subnet *s) {
	int scope, flag;
	scope = s->ip6.n16[0] & 0xf;
	flag  = (s->ip6.n16[0] >> 4) & 0xf;

	fprintf(out, "Scope : ");
	switch (scope) {
		case 1:
			fprintf(out, "Host Local\n");
			break;
		case 2:
			fprintf(out, "Link Local\n");
			break;
		case 5:
			fprintf(out, "Site Local\n");
			break;
		case 8:
			fprintf(out, "Organisation Local\n");
			break;
		case 14:
			fprintf(out, "Global\n");
			break;
		default:
			fprintf(out, "Invalid : %d\n", scope);
			break;
	}
	fprintf(out, "Flags : %d\n", flag);
	if (flag & 4)
		fprintf(out, "R=1, Embedded RP\n");
	else
		fprintf(out, "R=0, No Embedded RP\n");
	if (flag & 2)
		fprintf(out, "P=1, based on network prefix\n");
	else
		fprintf(out, "P=0, not based on network prefix\n");
	if (flag & 1)
		fprintf(out, "T=1, dynamically assigned prefix\n");
	else
		fprintf(out, "T=0, well-known multicast address\n");
	if (flag == 7)
		decode_ipv6_embedded_rp(out, s);
}

struct known_subnet_desc ipv4_known_subnets[] = {
	{&ipv4_default,		"IPv4 default address", -1},
	{&ipv4_broadcast,	"IPv4 broadcast address", -1},
	{&class_d,		"IPv4 multicast address", 1, &decode_ipv4_multicast},
	{&ipv4_unspecified,	"IPv4 unspecified address"},
	{&ipv4_rfc1918_1,	"IPv4 Private address (rfc1918)"},
	{&ipv4_rfc1918_2,	"IPv4 Private address (rfc1918)"},
	{&ipv4_rfc1918_3,	"IPv4 Private address (rfc1918)"},
	{&ipv4_loopback,	"IPv4 loopback address"},
	{&ipv4_rfc3927_ll,	"IPv4 link-local address (rfc6890)"},
	{&ipv4_rfc6598_sasr,	"IPv4 private Shared Address Space (rfc6598)"},
	{&ipv4_test1,		"IPv4 Test & doc NET (rfc5737)"},
	{&ipv4_test2,		"IPv4 Test & doc NET (rfc5737)"},
	{&ipv4_test3,		"IPv4 Test & doc NET (rfc5737)"},
	{&ipv4_bench,		"IPv4 benchmark test (rfc2544)"},
	{NULL, NULL}
};

struct known_subnet_desc ipv6_known_subnets[] = {
	{&ipv6_default,		"IPv6 default address", -1},
	{&ipv6_unspecified,	"IPv6 unspecified address"},
	{&ipv6_global,		"IPv6 global address"},
	{&ipv6_ula,		"IPv6 Unique Local Address"},
	{&ipv6_sitelocal,	"IPv6 Site Local address (deprecated)"},
	{&ipv6_linklocal,	"IPv6 link-local address"},
	{&ipv6_multicast,	"IPv6 multicast address", 1, &decode_ipv6_multicast},
	{&ipv6_6to4,		"IPv6 6to4", 1, &decode_6to4},
	{&ipv6_rfc4380_teredo,	"IPv6 rfc4380 Teredo", 1, &decode_teredo},
	{&ipv6_rfc3849_doc,	"IPv6 rfc3849 Documentation-reserved addresses"},
	{&ipv6_rfc6052_pat,	"IPv6 rfc6052 protocol address translation", 1, &decode_rfc6052},
	{&ipv6_isatap_priv_ll,	"IPv6 ISATAP link-local address", 1, &decode_isatap_ll},
	{&ipv6_isatap_pub_ll,	"IPv6 ISATAP link-local address", 1, &decode_isatap_ll},
	{&ipv6_mapped_ipv4,	"IPv6 Mapped-IPv4 address"},
	{&ipv6_compat_ipv4,	"IPv6 Compat-IPv4 address (Deprecated)"},
	{&ipv6_loopback,	"IPv6 Loopback Address"},
	{NULL, NULL}
};

const struct subnet ipv4_mcast_ll	= S_IPV4_CONST(224,0, 8);
const struct subnet ipv4_mcast_ssm	= S_IPV4_CONST(232,0, 8);
const struct subnet ipv4_mcast_glop	= S_IPV4_CONST(233,0, 8);
const struct subnet ipv4_mcast_site	= S_IPV4_CONST(239,0, 8);

static void decode_mcast_glop(FILE *out, const struct subnet *s) {
	fprintf(out, "Glop AS : %d\n", (s->ip >> 8) & 0xFFFF);
}
struct known_subnet_desc ipv4_mcast_known_subnets[] = {
	{&ipv4_mcast_ll,	"IPv4 Link-Local Multicast Addresses"},
	{&ipv4_mcast_ssm,	"IPv4 Source Specific Multicast Addresses"},
	{&ipv4_mcast_glop,	"IPv4 Glop Multicast Addresses (rfc2770)", 1, &decode_mcast_glop},
	{&ipv4_mcast_site,	"IPv4 Site Local (private) Multicast Addresses"},
	{NULL, NULL}
};

static void fprint_ip_membership(FILE *out, struct subnet *s) {
	int i, res;
	int found_i, found_mask;
	struct known_subnet_desc *k;

	if (s->ip_ver == IPV4_A)
		k = ipv4_known_subnets;
	else if (s->ip_ver == IPV6_A)
		k = ipv6_known_subnets;
	else
		return;
	found_mask = -1;
	for (i = 0; ;i++) {
		if (k[i].s == NULL)
			break;
		res = subnet_compare(s, k[i].s);
		st_debug(ADDRCOMP, 5, "%P against %P\n", *s, *k[i].s);
		if (res == EQUALS || (res == INCLUDED && k[i].always_print != -1)) {
			if ((int)k[i].s->mask >= found_mask) {
				found_mask = k[i].s->mask;
				found_i = i;
			}
		}
	}
	if (found_mask >= 0) {
		fprintf(out, "%s\n", k[found_i].desc);
		if (k[found_i].decode_more)
			k[found_i].decode_more(out, s);
	}
}

char ipv4_get_class(const struct subnet *s) {
	if ((s->ip >> 31) == 0)
		return 'a';
	if ((s->ip >> 30) == 2)
		return 'b';
	if ((s->ip >> 29) == 6)
		return 'c';
	if ((s->ip >> 28) == 14)
		return 'd';
	if ((s->ip >> 28) == 15)
		return 'e';
	return 0;
}

void fprint_ipv4_info(FILE *out, struct subnet *subnet) {
	char c;

	c = ipv4_get_class(subnet);
	fprintf(out, "Classfull info : class %c\n", c);
	fprint_ip_membership(out, subnet);
}

void fprint_ipv6_info(FILE *out, struct subnet *subnet) {
	fprint_ip_membership(out, subnet);
}

void fprint_ip_info(FILE *out, struct subnet *subnet) {
	fprintf(out, "IP version : %d\n", subnet->ip_ver);
	st_fprintf(out, "Network Address : %N/%m\n", *subnet, *subnet);
	st_fprintf(out, "Address   Range : %N - %B\n", *subnet, *subnet);
	st_fprintf(out, "Previous subnet : %L/%m\n", *subnet, *subnet);
	st_fprintf(out, "Next     subnet : %U/%m\n", *subnet, *subnet);
	if (subnet->ip_ver == IPV4_A)
		fprint_ipv4_info(out, subnet);
	if (subnet->ip_ver == IPV6_A)
		fprint_ipv6_info(out, subnet);
}
