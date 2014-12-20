#include <stdio.h>
#include <stdlib.h>
#include "iptools.h"
#include "debug.h"
#include "st_printf.h"


const struct subnet class_a = { .ip = 0, .mask = 1};
const struct subnet class_b = { .ip = (1 << 31), .mask = 2};
const struct subnet class_c = { .ip = (3 << 30), .mask = 3};
const struct subnet class_d = { .ip = (14 << 29), .mask = 4};
const struct subnet class_e = { .ip = (15 << 29), .mask = 4};


#define S_IPV4_CONST(DIGIT1, DIGIT2, __MASK)  { .ip_ver = 4, .ip = (DIGIT1 << 24) + (DIGIT2 << 16), .mask = __MASK }
const struct subnet ipv4_rfc1918_1 = S_IPV4_CONST(10, 0, 8);
const struct subnet ipv4_rfc1918_2 = S_IPV4_CONST(172, 16, 12);
const struct subnet ipv4_rfc1918_3 = S_IPV4_CONST(192, 168, 16); 
const struct subnet ipv4_loopback = S_IPV4_CONST(127, 0, 8); 
const struct subnet ipv4_rfc3927_ll = S_IPV4_CONST(169, 254, 16); 
const struct subnet ipv4_rfc6598_sasr =S_IPV4_CONST(100, 64, 10); 


#define S_IPV6_CONST(DIGIT1, DIGIT2, __MASK)  { .ip_ver = 6, .ip6.n16[0] = DIGIT1, .ip6.n16[1] = DIGIT2, .mask = __MASK }
const struct subnet ipv6_6to4   = S_IPV6_CONST(0x2002, 0, 16);
const struct subnet ipv6_rfc4380_teredo = S_IPV6_CONST(0x2001, 0, 32);
const struct subnet ipv6_rfc3849_doc = S_IPV6_CONST(0x2001, 0x0DB8, 32);
const struct subnet ipv6_isatap = {.ip_ver = 6, .ip6.n16[0] = 0xFE80, .ip6.n16[5] = 0x5EFE, .mask = 96};
const struct subnet ipv6_mapped_ipv4 = {.ip_ver = 6, .ip6.n16[5] = 0xFFFF, .mask = 96};
const struct subnet ipv6_compatible_ipv4 = {.ip_ver = 6, .mask = 96};
const struct subnet ipv6_loopback = {.ip_ver = 6, .ip6.n16[7] = 1, .mask = 128};
void init_know_subnets() {
	/*
	get_subnet_or_ip("10.0.0.0/8", &rfc1918_1);
	get_subnet_or_ip("172.16.0.0/12", &rfc1918_2);
	get_subnet_or_ip("192.168.0.0/16", &rfc1918_3);
	get_subnet_or_ip("127.0.0.0/8", &ipv4_loopback);
	get_subnet_or_ip("169.254.0.0/16", &rfc3927_ipv4_ll);
	get_subnet_or_ip("100.64.0.0/10", &rfc6598_ipva4_sasr);
	*/

}

char ipv4_get_class(struct subnet *s) {
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
		return;
	}
}

void fprint_ipv6_info(FILE *out, struct subnet *subnet) {

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
