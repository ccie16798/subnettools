#ifndef IPTOOLs_H
#define IPTOOLS_H

#define EQUALS 1
#define INCLUDED 2
#define INCLUDES 3
#define NOMATCH -1

#define BAD_MASK 4012
#define BAD_IP 4012
#define IPV4_A 4
#define IPV6_A 6
#define IPV4_N 14 /*  IPV4_N must be IPV4_A + 10 */
#define IPV6_N 16 /*  IPV6_N must be IPV4_6 + 10 */


typedef unsigned int u32;
typedef unsigned int ipv4;

struct ipv6_a {
	union {
		unsigned char	n8[16];
		unsigned short	n16[8];
		u32  		n32[4];
	};
};

typedef struct ipv6_a ipv6;
struct subnet {
	int ip_ver;
	union {
        	ipv4 ip;
		ipv6 ip6;
	};
        u32 mask;
};

struct route {
	struct subnet subnet;
	char device[32];
	union {
		ipv4 gw;
		ipv6 gw6;
	};
        char comment[128];
};

struct subnet_file {
        struct route *routes;
        unsigned long nr;
        unsigned long max_nr; /* the number of routes that has been malloced */
};


int is_equal_ipv6(ipv6 ip1, ipv6 ip2);

int alloc_subnet_file(struct subnet_file *sf, unsigned long n) ;
void print_route(struct route r, FILE *output);
void print_subnet_file(struct subnet_file sf);
void fprint_subnet_file(struct subnet_file sf, FILE *output);
/*
 * compare sub1 & sub2 for inclusion
 * returns :
 * INCLUDES if  sub1 includes sub2
 * INCLUDED if  sub1 is included in sub2
 * EQUALS   if  sub1 equals sub2
 * -1 otherwise
 */
int subnet_compare(const struct subnet *sub1, const struct subnet *sub2);
int subnet_compare_ipv4(ipv4 ip1, u32 mask1, ipv4 ip2, u32 mask2);
int subnet_compare_ipv6(ipv6 ip1, u32 mask1, ipv6 ip2, u32 mask2);

int subnet_is_superior(struct subnet *s1, struct subnet *s2);

/* for the following functions, output buffer MUST be allocated by caller and large enough */
int subnet2str(const struct subnet *s, char *out_buffer);
int addrv42str(ipv4 z, char *out_buffer);
int addrv62str(ipv6 z, char *out_buffer, int compress);
/* un-compress an IPv6 address */
int ipv6expand(const char *input, char *out);

/* fill struct subnet from a string */
int get_subnet_or_ip(const char *s, struct subnet *subnet);
int get_single_ip(const char *s, struct subnet *subnet);
u32 string2mask(const char *s) ;

/* try to aggregate s1 & s2, putting the result 'in aggregated_subnet' if possible
 * returns negative if impossible to aggregate, positive if possible */
int aggregate_subnet(const struct subnet *s1, const struct subnet *s2, struct subnet *aggregated_subnet);
#else
#endif
