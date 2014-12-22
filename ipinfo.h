#ifndef IPINFO_H
#define IPINFO_H

void fprint_ip_info(FILE *, const struct subnet *);

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


#else
#endif
