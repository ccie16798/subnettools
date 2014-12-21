#include <stdio.h>
#include <stdlib.h>
#include "iptools.h"
#include "debug.h"
#include "st_printf.h"


const struct subnet class_a = { .ip_ver = IPV4_A, .ip = 0, .mask = 1};
const struct subnet class_b = { .ip_ver = IPV4_A, .ip = (1 << 31), .mask = 2};
const struct subnet class_c = { .ip_ver = IPV4_A, .ip = (3 << 30), .mask = 3};
const struct subnet class_d = { .ip_ver = IPV4_A, .ip = (14 << 29), .mask = 4};
const struct subnet class_e = { .ip_ver = IPV4_A,.ip = (15 << 29), .mask = 4};


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
const struct subnet ipv6_isatap_ll 	= {.ip_ver = 6, .ip6.n16[0] = 0xFE80, .ip6.n16[5] = 0x5EFE, .mask = 96};
const struct subnet ipv6_mapped_ipv4	= {.ip_ver = 6, .ip6.n16[5] = 0xFFFF, .mask = 96}; /* ::FFFF:/96 */
const struct subnet ipv6_compat_ipv4	= {.ip_ver = 6, .mask = 96}; /* ::/96 */
const struct subnet ipv6_loopback 	= {.ip_ver = 6, .ip6.n16[7] = 1, .mask = 128}; /* ::1/128 */

void decode_6to4(FILE *out, struct subnet *s) {
	fprintf(out, "6to4 IPv4 destination address : %d.%d.%d.%d\n", s->ip6.n16[1] >> 8, s->ip6.n16[1] & 0xFF,
			s->ip6.n16[2] >> 8, s->ip6.n16[2] & 0xFF);

}

void decode_teredo(FILE *out, struct subnet *s) {
	fprintf(out, "Teredo server : %d.%d.%d.%d\n", s->ip6.n16[2] >> 8, s->ip6.n16[2] & 0xFF,
                        s->ip6.n16[3] >> 8, s->ip6.n16[3] & 0xFF);
	fprintf(out, "Client IP     : %d.%d.%d.%d\n", (s->ip6.n16[6] >> 8) ^ 0xFF , (s->ip6.n16[6] & 0xFF) ^ 0xFF,
                        (s->ip6.n16[7] >> 8) ^ 0xFF, (s->ip6.n16[7] & 0xFF) ^ 0xFF);

	fprintf(out, "UDP port      : %d\n", s->ip6.n16[5] ^ 0xFFFF);
}

void decode_rfc6052(FILE *out, struct subnet *s) {
	fprintf(out, "IPv4-Embedded IPv6 address : 64:ff9b::%d.%d.%d.%d\n", s->ip6.n16[6] >> 8, s->ip6.n16[6] & 0xFF,
			s->ip6.n16[7] >> 8, s->ip6.n16[7] & 0xFF);
}

struct known_subnet_desc {
	const struct subnet *s;
	char *desc;
	int always_print; /*-1 : print on EQUALS only, 
			    0 :print on any match,
			    1 : print even if it is not the longest match */
	void (*decode_more)(FILE *out, struct subnet *s);
};

struct known_subnet_desc ipv4_known_subnets[] = {
	{&ipv4_default,		"IPv4 default address", -1},
	{&ipv4_broadcast,	"IPv4 broadcast address", -1},
	{&class_d,		"IPv4 multicast address"},
	{&ipv4_unspecified,	"IPv4 unspecified address"},
	{&ipv4_rfc1918_1,	"Private IP address (rfc1918)"},
	{&ipv4_rfc1918_2,	"Private IP address (rfc1918)"},
	{&ipv4_rfc1918_3,	"Private IP address (rfc1918)"},
	{&ipv4_loopback,	"IPv4 loopback addresses"},
	{&ipv4_rfc3927_ll,	"IPv4 link-local addresses (rfc6890)"},
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
	{&ipv6_ula,		"IPv6 Unique Local Addresses"},
	{&ipv6_sitelocal,	"IPv6 Site Local addresses (deprecated)"},
	{&ipv6_linklocal,	"IPv6 link-local addresses"},
	{&ipv6_multicast,	"IPv6 multicast addresses"},
	{&ipv6_6to4,		"IPv6 6to4", 1, &decode_6to4},
	{&ipv6_rfc4380_teredo,	"IPv6 rfc4380 Teredo", 1, decode_teredo},
	{&ipv6_rfc3849_doc,	"IPv6 rfc3849 Documentation-reserved addresses"},
	{&ipv6_rfc6052_pat,	"IPv6 rfc6052 protocol address translation", 1, decode_rfc6052},
	{&ipv6_isatap_ll,	"IPv6 ISATAP link-local addresses"},
	{&ipv6_isatap_ll,	"IPv6 ISATAP link-local addresses"},
	{&ipv6_mapped_ipv4,	"IPv6 Mapped-IPv4 addresses"},
	{&ipv6_compat_ipv4,	"IPv6 Compat-IPv4 addresses (Deprecated)"},
	{&ipv6_loopback,	"IPv6 Loopback Address"},
	{NULL, NULL}
};

void fprint_ip_membership(FILE *out, struct subnet *s) {
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

void fprint_multicast_ipv4_info(FILE *out, struct subnet *subnet) {

}

void fprintf_know_ipv4_ranges(FILE *out, struct subnet *subnet) {


}

void fprint_ipv4_info(FILE *out, struct subnet *subnet) {
	char c;

	c = ipv4_get_class(subnet);
	fprintf(out, "Classfull info : class %c\n", c);
	if (c == 'd') {
		fprint_multicast_ipv4_info(out, subnet);
		return;
	}
	if (c == 'e') {
		fprintf(out, "Unassigned address range\n");
	}
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
