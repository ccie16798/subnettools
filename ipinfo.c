#include <stdio.h>
#include <stdlib.h>
#include "iptools.h"
#include "debug.h"
#include "st_printf.h"


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
