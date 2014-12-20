#include <stdio.h>
#include <stdlib.h>
#include "iptools.h"
#include "debug.h"
#include "st_printf.h"

void fprint_ipv4_info(FILE *out, struct subnet *subnet) {

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
