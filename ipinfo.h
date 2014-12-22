#ifndef IPINFO_H
#define IPINFO_H

struct known_subnet_desc {
	const struct subnet *s;
	char *desc;
	int always_print; /*-1 : print on EQUALS only, 
			    0 :print on any match,
			    1 : print even if it is not the longest match */
	void (*decode_more)(FILE *out, const struct subnet *s);
};


extern struct known_subnet_desc ipv4_known_subnets[];
extern struct known_subnet_desc ipv6_known_subnets[];
extern struct known_subnet_desc ipv4_mcast_known_subnets[];

void fprint_ip_info(FILE *, const struct subnet *);

void fprint_ipv6_known_subnets(FILE *out);
void fprint_ipv4_known_subnets(FILE *out);
#else
#endif
