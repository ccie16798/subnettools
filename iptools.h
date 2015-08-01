#ifndef IPTOOLS_H
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
		/* beware of endianness issues
		 * current version of subnet tool manipulate ->n16 only, use n32 your own risk
		 */
		unsigned short	n16[8];
		u32  		n32[4];
	};
};
typedef struct ipv6_a ipv6;

/* due to endianness issues, ipv6 address should not be manipulated directly
 * works today, but will break the day we use something like a u128 to do the math...
 * if you change the representation of IPv6, you must redefine these macro,
 * (and only these) all code in .c file is safe
 */
#ifndef BOGUS_U128

#define block(__ip6, __n) __ip6.n16[__n]
#define set_block(__ip6, __n, __value) __ip6.n16[__n] = __value
#define block_OR(__ip6, __n, __value) __ip6.n16[__n] |= __value

#define shift_ipv6_left(__z, __len) shift_left(__z.n16, 8, __len)
#define shift_ipv6_right(__z, __len) shift_right(__z.n16, 8, __len)
#define increase_ipv6(__z) increase_bitmap(__z.n16, 8)
#define decrease_ipv6(__z) decrease_bitmap(__z.n16, 8)

#else
/* this so you get the idea ....*/
#define block(__ip6, __n) (u16)((__ip6 >> ((7 - __n) * 16)) & 0xFF)
#define set_block(__ip6, __n, __value) do { \
	u16 cur = (__ip6 >> ((7 - __n) * 16)) & 0xFF; \
	cur ^= __value; \
	__ip6 ^= (cur << ((7 - __n) * 16)); \
} while (0); /* not sure about this one :) */
#define shift_ipv6_left(__z, __len) __z <<= __n
#define shift_ipv6_right(__z, __len) __z >>= __n
#define increase_ipv6(__z) __z++
#define decrease_ipv6(__z) __z--

#endif

struct ip_addr {
	int ip_ver;
	union {
		ipv4 ip;
		ipv6 ip6;
	};
};

struct subnet {
	union {
		struct {
			int ip_ver;
			union {
				ipv4 ip;
				ipv6 ip6;
			};
		};
		struct ip_addr ip_addr;
	};
	u32 mask;
};

struct route {
	struct subnet subnet;
	char device[32];
	struct ip_addr gw;
	char comment[128];
};

struct bgp_route {
	struct subnet subnet;
	struct ip_addr gw;
	int MED;
	int LOCAL_PREF;
	char AS_PATH[256];
	int type; /* eBGP or iBGP */
	int weight;
	int best;
	int valid;
};

struct subnet_file {
	struct route *routes;
	unsigned long nr;
	unsigned long max_nr; /* the number of routes that has been malloced */
};

struct bgp_file {
	struct bgp_route *routes;
	unsigned long nr;
	unsigned long max_nr; /* the number of routes that has been malloced */
};

int is_ip_char(char c);
void copy_ipaddr(struct ip_addr *a, const struct ip_addr *b);
void copy_subnet(struct subnet *a, const struct subnet *b);
void copy_route(struct route *a, const struct route *b);
int is_equal_ipv6(ipv6 ip1, ipv6 ip2);
int is_equal_ip(struct ip_addr *ip1, struct ip_addr *p2);
int is_equal_gw(struct route *r1, struct route *r2);

int ipv6_is_link_local(ipv6 a);
int ipv6_is_global(ipv6 a);
int ipv6_is_ula(ipv6 a);
int ipv6_is_multicast(ipv6 a);

int alloc_subnet_file(struct subnet_file *sf, unsigned long n) ;
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

int subnet_is_superior(const struct subnet *s1, const struct subnet *s2);

/* address to addr converting functions
   output buffer MUST be allocated by caller and large enough */
int addrv42str(ipv4 z, char *out_buffer, size_t len);
int addrv62str(ipv6 z, char *out_buffer, size_t len, int compress);
int subnet2str(const struct subnet *s, char *out_buffer, size_t len, int comp_level);
int addr2str(const struct ip_addr *a, char *out_buffer, size_t len, int comp_level);
int mask2ddn(u32 mask, char *out_buffer, size_t len);


/* read len chars from 's' and try to convert to a struct ip_addr
 * s doesnt need to be '\0' ended
 * returns :
 *    IPV4_A if addr is valid IPv4
 *    IPV6_A if addr is valid IPv6
 *    BAD_IP otherwise
 * */
int string2addr(const char *s, struct ip_addr *addr, int len);

/* read len chars from 's' and try to convert to a subnet mask length
 * s doesnt need to be '\0' ended
 * returns :
 *    'mask length' is 's' is a valid mask
 *     BAD_MASK otherwise
*/
u32 string2mask(const char *s, int len) ;


/* fills struct subnet from a string
 * returns :
 *    IPV4_A : IPv4 without mask
 *    IPV4_N : IPv4 + mask
 *    IPV6_A : IPv6 without mask
 *    IPV6_N : IPv6 +  mask
 *    BAD_IP, BAD_MASK on error
 */
int get_subnet_or_ip(const char *s, struct subnet *subnet);

/* variant of get_subnet_or_ip that allows IPv4 address to not print 'useless 0' like 10/8, 172.20/16, 192.168.1/24 */
int classfull_get_subnet(const char *string, struct subnet *subnet);

/* try to aggregate s1 & s2, putting the result 'in aggregated_subnet' if possible
 * returns negative if impossible to aggregate, positive if possible */
int aggregate_subnet(const struct subnet *s1, const struct subnet *s2, struct subnet *aggregated_subnet);

/* those 2 function DO MODIFY their argument */
void first_ip(struct subnet *s);
void last_ip(struct subnet *s);

void previous_subnet(struct subnet *s);
void next_subnet(struct subnet *s);

/* will return the largest number X where network_address(IP, mask) == network_address(IP, mask - X)
 * for example f(10.1.4.0/24) will return 2 since :
 *    network_address(10.1.4.0/22) == 10.1.4.0/22 (OK)
 *    network_address(10.1.4.0/21) == 10.1.0.0/21 (NOT OK)
 * in a bitmap, that is the number of right most 0 in (IP >> mask)
 * let's hope you followed me :)
 */
int can_decrease_mask(const struct subnet *s);
/*
 * remove s2 from s1 if possible
 * alloc a new struct subnet *
 * number of element is stored in *n
 */
struct subnet *subnet_remove(const struct subnet *s1, const struct subnet *s2, int *n);
#else
#endif
