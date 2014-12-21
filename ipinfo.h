#ifndef IPINFO_H
#define IPINFO_H

void fprint_ip_info(FILE *, struct subnet *);

extern struct known_subnet_desc ipv4_known_subnets[];
extern struct known_subnet_desc ipv6_known_subnets[];
extern struct known_subnet_desc ipv4_mcast_known_subnets[];

#else
#endif
