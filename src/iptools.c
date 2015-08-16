/*
 *  IPv4, IPv6 functions (compare, conversion to/from string, aggregate etc...)
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
#include <ctype.h>
#include <stdarg.h>
#include "debug.h"
#include "iptools.h"
#include "utils.h"
#include "bitmap.h"
#include "heap.h"
#include "st_printf.h"

sprint_hex(short)

inline int is_ip_char(char c) {
	return isxdigit(c) || c == ':' || c == '.';
}

inline void copy_ipaddr(struct ip_addr *a, const struct ip_addr *b) {
	memcpy(a, b, sizeof(struct ip_addr));
}

inline void copy_subnet(struct subnet *a, const struct subnet *b) {
	memcpy(a, b, sizeof(struct subnet));
}

inline void copy_route(struct route *a, const struct route *b) {
	memcpy(a, b, sizeof(struct route));
}

inline void zero_route(struct route *a) {
	memset(a, 0, sizeof(struct route));
}

inline void zero_ipaddr(struct ip_addr *a) {
	memset(a, 0, sizeof(struct ip_addr));
}
/*
 * compare sub1 & sub2 for inclusion
 * returns :
 * INCLUDES if  sub1 includes sub2
 * INCLUDED if  sub1 is included in sub2
 * EQUALS   if  sub1 equals sub2
 * -1 otherwise
 */
int subnet_compare(const struct subnet *sub1, const struct subnet *sub2) {
	if (sub1->ip_ver != sub2->ip_ver) {
		debug(ADDRCOMP, 2, "different address FAMILY\n");
		return -1;
	}
	if (sub1->ip_ver == IPV4_A)
		return subnet_compare_ipv4(sub1->ip, sub1->mask, sub2->ip, sub2->mask);
	else if (sub1->ip_ver == IPV6_A)
		return subnet_compare_ipv6(sub1->ip6, sub1->mask, sub2->ip6, sub2->mask);
	debug(ADDRCOMP, 1, "Impossible to get here, IP version = %d BUG?\n", sub1->ip_ver);
	return -1;
}

int is_equal_ipv6(ipv6 ip1, ipv6 ip2) {
	int i;

	for (i = 0; i < 4; i++)
		if (ip1.n32[i] != ip2.n32[i])
			return 0;
	return 1;
}
int is_equal_ip(struct ip_addr *ip1, struct ip_addr *ip2) {
	if (ip1->ip_ver != ip2->ip_ver)
		return 0;
	if (ip1->ip_ver == IPV4_A && ip1->ip == ip2->ip)
		return 1;
	if (ip1->ip_ver == IPV6_A && is_equal_ipv6(ip1->ip6, ip2->ip6))
		return 1;
	return 0;
}

int is_equal_gw(struct route *r1, struct route *r2) {
	if (r1->gw.ip_ver != r2->gw.ip_ver)
		return 0;
	if (r1->gw.ip_ver == 0) /* unset GW*/
		return 1;
	if (r1->gw.ip_ver == IPV4_A && r1->gw.ip == r2->gw.ip)
		return 1;
	else if (r1->gw.ip_ver == IPV6_A) {
		if (!is_equal_ipv6(r1->gw.ip6, r2->gw.ip6))
			return 0;
		if (!ipv6_is_link_local(r1->gw.ip6))
			return 1;
		/* if link local adress, we must check if the device is the same */
		return !strcmp(r1->device, r2->device);
	}
	return 0;
}

int ipv6_is_link_local(ipv6 a) {
	unsigned short x = block(a, 0);
/* link_local address is FE80::/10 */
	return ((x >> 6) == (0xFE80 >> 6));
}

int ipv6_is_global(ipv6 a) {
	unsigned short x = block(a, 0);
/* global address is 2000::/3 */
	return ((x >> 13) == (0x2000 >> 13));
}

int ipv6_is_ula(ipv6 a) {
	unsigned short x = block(a, 0);
/* ula address is FC00::/7 */
	return ((x >> 9)  == (0xFC00 >> 9));
}

int ipv6_is_multicast(ipv6 a) {
	/* IPv6 Multicast is FF00/8 */
	return ((block(a, 0) >> 8) == (0xFF00 >> 8));
}

int subnet_compare_ipv6(ipv6 ip1, u32 mask1, ipv6 ip2, u32 mask2) {
	if (mask1 > mask2) {
		shift_ipv6_right(ip1, (128 - mask2));
		shift_ipv6_right(ip2, (128 - mask2));
		/*print_bitmap(ip1.n16, 8);
		print_bitmap(ip2.n16, 8);	*/
		if (is_equal_ipv6(ip1, ip2))
			return INCLUDED;
		else
			return NOMATCH;
        } else if (mask1 < mask2) {
		shift_ipv6_right(ip1, (128 - mask1));
		shift_ipv6_right(ip2, (128 - mask1));
		/*print_bitmap(ip1.n16, 8);
		print_bitmap(ip2.n16, 8); */
                if (is_equal_ipv6(ip1, ip2))
                        return INCLUDES;
                else
                        return NOMATCH;
        } else {
		shift_ipv6_right(ip1, (128 - mask1));
		shift_ipv6_right(ip2, (128 - mask1));
                if (is_equal_ipv6(ip1, ip2))
                        return EQUALS;
                else
                        return NOMATCH;
        }
}

int subnet_compare_ipv4(ipv4 prefix1, u32 mask1, ipv4 prefix2, u32 mask2) {
	ipv4 a, b;

	if (mask1 > mask2) {
		a = prefix1 >> (32 - mask2);
		b = prefix2 >> (32 - mask2);
		if (a == b)
			return INCLUDED;
		else
			return NOMATCH;
	} else if  (mask1 < mask2) {
		a = prefix1 >> (32 - mask1);
		b = prefix2 >> (32 - mask1);
		if (a == b)
			return INCLUDES;
		else
			return NOMATCH;
	} else {
		a = prefix1 >> (32 - mask1);
		b = prefix2 >> (32 - mask1);
		if (a == b)
			return EQUALS;
		else
			return NOMATCH;
	}
}

int addrv42str(ipv4 z, char *out_buffer, size_t len) {
	unsigned int a, b, c, d;

	/*
	 * instead of using snprint to check outbuff isnt overrun, we make sure output buffer is large enough
	 * we refuse to print potentially truncated IPs and BUG early ; min size is (3 + 1) * 4
	 */
	if (len < 16) {
		fprintf(stderr, "BUG, %s needs at least a 16-bytes buffer\n", __FUNCTION__);
		out_buffer[0] = '\0';
		return -1;
	}
	d = z % 256;
	c = (z >> 8) % 256;
	b = (z >> 16) % 256;
	a = (z >> 24) % 256;
	/* SAFE to use sprintf here, we made sure buffer is large enough */
	return sprintf(out_buffer, "%d.%d.%d.%d", a, b, c, d);
}

/*
 * store human readable of IPv6 z in output
 * output MUST be large enough
 * compress = 0 ==> no adress compression
 * compress = 1 ==> remove leading zeros
 * compress = 2 ==> FULL compression but doesnt convert Embedded IPv4
 * compress = 3 ==> FULL compression and convert Embedded IPv4
 */
int addrv62str(ipv6 z, char *out_buffer, size_t len, int compress) {
	int a, i, j;
	int skip = 0, max_skip = 0;
	int skip_index = 0, max_skip_index = 0;

	/*
	 * instead of using snprint to check outbuff isnt overrun at each step, we make sure output buffer is large enough
	 * we refuse to print potentially truncated IPs and BUG early ; min size if (4 + 1) * 8
	 */
	if (len < 40) {
		fprintf(stderr, "BUG, %s needs at least a 40-bytes buffer\n", __FUNCTION__);
		out_buffer[0] = '\0';
		return -1;
	}
	if (compress == 0) {
		a = sprintf(out_buffer, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x", block(z, 0), block(z, 1) , block(z, 2) , block(z, 3) , block(z, 4), block(z, 5), block(z, 6), block(z, 7));
		return a;
	} else if (compress == 1) {
		a = sprintf(out_buffer, "%x:%x:%x:%x:%x:%x:%x:%x", block(z,0), block(z, 1) , block(z, 2) , block(z, 3) , block(z, 4), block(z, 5), block(z, 6), block(z, 7));
		return a;
	}
	/**
	 ** longest 0000 block sequence will be replaced
	 */
	for (i = 0; i < 8; i++) {
		if (block(z, i) == 0) {
			if (skip == 0)  {
				debug(PARSEIPV6, 8, "possible skip index %d\n", i);
				skip_index = i;
			}
			skip++;
		} else {
			if (skip) {
				/* in case of egality, we prefer to replace block to the right */
				/* if you want to replace the left block change the comparison to '>' */
				if (skip >= max_skip) {
					debug(PARSEIPV6, 8, "skip index %d better \n", skip_index);
					max_skip = skip;
					max_skip_index = skip_index;
				}
				skip = 0;
			}
		}
	}
	if (skip && (skip >= max_skip)) {
		/* happens in case address ENDS with :0000, we then left the loop without setting max_skip__ */
		max_skip =  skip;
		max_skip_index = skip_index;
	}
	if (max_skip == 1) /* do not bother to compress if there is only one block to compress */
		max_skip = max_skip_index = 0;
	debug(PARSEIPV6, 5, "can skip %d blocks at index %d\n", max_skip, max_skip_index);
	if (compress == 3 && (skip_index == 0 && (max_skip >= 5 && max_skip < 8))) {
		/* Mapped& Compatible IPv4 address */
		if (block(z, 5) == 0x0) {
			if (block(z, 6) == 0 && block(z, 7) == 1) /** the loopback address */
				return sprintf(out_buffer, "::1");
			else
				return sprintf(out_buffer, "::%d.%d.%d.%d", block(z, 6) >> 8, block(z, 6) & 0xff,
						block(z, 7) >> 8, block(z, 7) & 0xff);
		}
		if (block(z, 5) == 0xffff)
			return sprintf(out_buffer, "::ffff:%d.%d.%d.%d", block(z, 6) >> 8, block(z, 6) & 0xff,
					block(z, 7) >> 8, block(z, 7) & 0xff);
	}
	j = 0;
	for (i = 0; i < max_skip_index; i++) {
		j += sprint_hexshort(out_buffer + j, block(z, i));
		out_buffer[j++] = ':';
	}
	if (max_skip > 0) {
		out_buffer[j++] = ':';
		if (!max_skip_index) /* means addr starts with 0 */
			out_buffer[j++] = ':';
	}
	for (i = max_skip_index + max_skip; i < 7; i++) {
		j += sprint_hexshort(out_buffer + j, block(z, i));
		out_buffer[j++] = ':';
	}
	if (i < 8)
		j += sprint_hexshort(out_buffer + j, block(z, i));
	out_buffer[j] = '\0';
	return j;
}

/* outbuffer must be large enough **/
int subnet2str(const struct subnet *s, char *out_buffer, size_t len, int comp_level) {
	if (s->ip_ver == IPV4_A)
		return addrv42str(s->ip, out_buffer, len);
	if (s->ip_ver == IPV6_A)
		return addrv62str(s->ip6, out_buffer, len, comp_level);
	out_buffer[0] = '\0';
	return -1;
}

/* outbuffer must be large enough **/
int addr2str(const struct ip_addr *a, char *out_buffer, size_t len, int comp_level) {
	if (a->ip_ver == IPV4_A)
		return addrv42str(a->ip, out_buffer, len);
	if (a->ip_ver == IPV6_A)
		return addrv62str(a->ip6, out_buffer, len, comp_level);
	out_buffer[0] = '\0';
	return -1;
}
/* IPv4 only, mask to Dot Decimal Notation */
int mask2ddn(u32 mask, char *out, size_t len) {
	int i;
	unsigned int s[4];

	if (len < 16) {
		fprintf(stderr, "BUG, %s needs at least a 16-bytes buffer\n", __FUNCTION__);
		out[0] = '\0';
		return -1;
	}
	for (i = 0; i < mask / 8; i++)
		s[i] = 255;
	if (i < 4) {
		s[i++] = 256 - (1 << (8 - (mask % 8)));
		for ( ; i < 4; i++)
			s[i] = 0;
	}
	return  sprintf(out, "%d.%d.%d.%d", s[0], s[1], s[2], s[3]);
}

int string2mask(const char *s, int len) {
	int i = 0;
	u32 a;
	int count_dot = 0;
	int truc[4];

	if (s[i] == '/') /* leading / is permitted */
		i++;
	if (s[i] == '\0') {
		debug(PARSEIP, 3, "Invalid mask '%s', no digits found\n", s);
		return BAD_MASK;
	}
	if (s[i] == '.') {
		debug(PARSEIP, 3, "Invalid mask '%s', starts with '.'\n", s);
		return BAD_MASK;
	}
	for ( ; ; i++) {
		if (s[i] == '\0' || i == len) {
			break;
		} else if (s[i] == '.') {
		 	if (s[i + 1] == '.') {
				debug(PARSEIP, 3, "Invalid mask '%s', contains consecutive '.'\n", s);
				return BAD_MASK;
			}
			if (s[i + 1] == '\0') {
				debug(PARSEIP, 3, "Invalid mask '%s', ends with '.'\n", s);
				return BAD_MASK;
			}
			count_dot++;
		} else if (isdigit(s[i]))
			continue;
		else {
			debug(PARSEIP, 3, "Invalid mask '%s', contains '%c'\n", s, s[i]);
			return BAD_MASK;
		}
	}
	if (count_dot == 0) { /* prefix length*/
		a = atoi(s);
		if (a > 128) {
			debug(PARSEIP, 3, "Invalid mask '%s', too long\n", s);
			return BAD_MASK;
		}
		return a;
	}
	if (count_dot != 3) {
		debug(PARSEIP, 3, "Invalid mask '%s', bad format\n", s);
		return BAD_MASK;
	}
	a = 0;
	sscanf(s, "%d.%d.%d.%d", truc, truc + 1, truc + 2, truc + 3);
	for (i = 0; i < 4; i++) {
		if (i && ( (truc[i] > truc[i - 1])  || (truc[i] && truc[i] < 255 && truc[i - 1] < 255))   ) {
			debug(PARSEIP, 3, "Invalid X.X.X.X mask %s\n",s);
			return BAD_MASK;
		}
		if (!isPower2(256 - truc[i])) {
			debug(PARSEIP, 3, "Invalid X.X.X.X mask, contains '%d'\n", truc[i]);
			return BAD_MASK;
		}
		a += 8 - mylog2(256 - truc[i]);
	}
	return a;
}

static int string2addrv4(const char *s, struct ip_addr *addr, int len) {
	int i;
	int count_dot = 0;
	int truc[4];
	int current_block = 0;

	if (*s == '\0') {
		debug(PARSEIP, 3, "Invalid IP '%s', null\n", s);
		return BAD_IP;
	}
	if (*s == '.') {
		debug(PARSEIP, 3, "Invalid IP '%s', starts with '.'\n", s);
		return BAD_IP;
	}
	for (i = 0; ; i++) {
		if (s[i] == '\0' || i == len) {
			truc[count_dot] = current_block;
			if (current_block > 255) {
				debug(PARSEIP, 3, "Invalid IP '%s', %d too big\n", s, current_block);
				return BAD_IP;
			}
			break;
		} else if (s[i] == '.') {
			if  (s[i + 1] == '.') {
				debug(PARSEIP, 3, "Invalid IP '%s', contains 2 consecutives '.'\n", s);
				return BAD_IP;
			}
			if  (s[i + 1] == '\0') {
				debug(PARSEIP, 3, "Invalid IP '%s', ends with '.'\n", s);
				return BAD_IP;
			}
			if (current_block > 255) {
				debug(PARSEIP, 3, "Invalid IP '%s', %d too big\n", s, current_block);
				return BAD_IP;
			}
			if (count_dot == 3) {
				debug(PARSEIP, 3, "Invalid IP '%s', too many '.'\n", s);
				return BAD_IP;
			}
			truc[count_dot] = current_block;
			count_dot++;
			current_block = 0;
		} else if (isdigit(s[i])) {
			current_block *= 10;
			current_block += s[i] - '0';
			continue;
		} else {
			debug(PARSEIP, 3, "Invalid IP '%s',  contains '%c'\n", s, s[i]);
			return BAD_IP;
		}
	}
	if (count_dot != 3) {
		debug(PARSEIP, 3, "Invalid IP '%s', not enough '.'\n", s);
		return BAD_IP;
	}
	addr->ip = (truc[0] << 24) + (truc[1] << 16) + (truc[2] << 8) + truc[3];
	addr->ip_ver = IPV4_A;
	return IPV4_A;
}

static int string2addrv6(const char *s, struct ip_addr *addr, int len) {
	int i, j, k;
	int do_skip = 0;
	int out_i = 0;
	int stop = 0;
	int count = 0, count2 = 0, count_dot = 0;
	int try_embedded, skipped_blocks;
	unsigned short current_block;
	int num_digit;
	struct ip_addr embedded; /* in case of a  IPv4-Compatible IPv6 or IPv4-Mapped IPv6 */

	if (s[0] == ':') { /** loopback addr **/
		if (s[1] != ':') {
			debug(PARSEIPV6, 3, "Invalid IPv6 address '%s', cannot begin with a single ':'\n", s);
			return BAD_IP;
		}
		do_skip = 1;
		if (s[2] == '\0' || len == 2) { /* special :: */
			memset(&addr->ip6, 0, sizeof(addr->ip6));
			addr->ip_ver = IPV6_A;
			return IPV6_A;
		}
		count2++;
	} else if (s[0] == '.') {
		/* we should not get here since addr2str calls addrv42str if it counts '.' */
		debug(PARSEIPV6, 3, "Invalid IPv6 address '%s', starts with '.'\n", s);
		return BAD_IP;
	}
	/**  couting ':' (7max),  '::' (1max), and '.' (0 or 3) */
	for (i = 1;  i < len; i++) {
		if (count_dot && count_dot != 3 && s[i] == ':') {
			debug(PARSEIPV6, 3, "Invalid IPv6 address '%s', found a ':' after only %d '.', cannot build embedded IP\n", s, count_dot);
			return BAD_IP;
		} else if (s[i] == ':') {
			count++;
			if (i != len - 1 && s[i + 1] == ':')
				count2++;
		} else if (s[i] == '.') {
			if (s[i - 1] == ':') {
				debug(PARSEIPV6, 3, "Invalid IP '%s', found '.' after ':'\n", s);
				return BAD_IP;
			}
			if (i == len - 1 || s[i + 1] == '\0') {
				debug(PARSEIPV6, 3, "Invalid IP '%s', ends with '.'\n", s);
				return BAD_IP;
			}
			if (s[i + 1] == '.') {
				debug(PARSEIPV6, 3, "Invalid IP '%s', contains 2 consecutives '.'\n", s);
				return BAD_IP;
			}
			count_dot++;
		} else if (s[i] == '\0')
			break;
	}
	/* getting the correct number of :, :: etc **/
	if (count2 > 1) {
		debug(PARSEIPV6, 3, "Invalid IPv6 address '%s', too many '::', [%d]\n", s, count2 );
		return BAD_IP;
	}
	if (count_dot) {
		if  (count_dot != 3) {
			debug(PARSEIPV6, 3, "Invalid IPv4-Embedded/Mapped address '%s', wrong '.' count, [%d]\n", s, count_dot);
			return BAD_IP;
		}
		if (count > 6) {
			debug(PARSEIPV6, 3, "Invalid IPv6 address '%s', too many ':', [%d]\n", s, count );
			return BAD_IP;
		}
		if (count2 == 0 && count != 6) {
			debug(PARSEIPV6, 3, "Invalid IPv4-Embedded/Mapped address '%s', bad ':' count, [%d]\n", s, count );
			return BAD_IP;
		}
		skipped_blocks = 7 - count;
	} else {
		if (count > 7) {
			debug(PARSEIPV6, 3, "Invalid IPv6 address '%s', too many ':', [%d]\n", s, count );
			return BAD_IP;
		}
		if (count2 == 0 && count != 7) {
			debug(PARSEIPV6, 3, "Invalid IPv6 address '%s', not enough ':', [%d]\n", s, count );
			return BAD_IP;
		}
		skipped_blocks = 8 - count;
	}

	debug(PARSEIPV6, 8, "counted %d ':', %d '::', %d '.'\n", count, count2, count_dot);
	i = (do_skip ? 1 : 0); /* in case we start with :: */
	current_block = 0;
	num_digit = 0;
	debug(PARSEIPV6, 8, "still to parse %s\n", s + i);
	for (;; i++) {
		if (do_skip) {
			/* we refill the skipped 0000: blocks */
			debug(PARSEIPV6, 9, "copying %d skipped '0000:' blocks\n", skipped_blocks);
			for (j = 0; j < skipped_blocks; j++) {
				set_block(addr->ip6, out_i, 0);
				out_i++;
			}
			do_skip = 0;
			current_block = 0;
			num_digit = 0;
		} else if (s[i] ==':'||s[i] == '\0'||i == len) {
			if (s[i] == '\0'||i == len)
				stop = 1;
			debug(PARSEIPV6, 8, "copying '%x' to block#%d\n", current_block, out_i);
			set_block(addr->ip6, out_i, current_block);
			if (stop) /* we are here because s[i] was 0 before we replaced it*/
				break;

			out_i++;
			current_block = 0;
			num_digit = 0;
			if (s[i + 1] == ':') /* we found a ':: (compressed address) */
				do_skip = 1;
			debug(PARSEIPV6, 9, "still to parse '%s', %d blocks already parsed\n", s + i + 1, out_i);
		} else if (isxdigit(s[i])) {
			if (num_digit == 4) {
				debug(PARSEIPV6, 3, "Invalid IPv6 '%s', block#%d has too many chars\n", s, out_i);
				return BAD_IP;

			}
			current_block <<= 4;
			current_block += char2int(s[i]);
			num_digit++;
			continue;
		} else {
			debug(PARSEIPV6, 3, "Invalid char '%c' found in block#%d\n", s[i], out_i);
			return BAD_IP;
		}
		if (out_i == 6 && count_dot) { /* try to see if it is a ::ffff:IPv4 or ::Ipv4 */
			try_embedded = 1;
			for (k = 0 ; k < 5; k++) {
				if (block(addr->ip6, k) != 0) {
					try_embedded = 0;
					break;
				}
			}
			if (try_embedded == 0)
				continue;
			if (block(addr->ip6, 5) != 0 && block(addr->ip6, 5) != 0xffff)
				continue;

			debug(PARSEIPV6, 9, "'%s' MAY be an embedded/mapped IPv4\n", s + i + 1);
			if (string2addrv4(s + i + 1, &embedded, 20) == IPV4_A) {
				debug(PARSEIPV6, 9, "'%s' is an embedded/mapped IPv4\n", s + i + 1);
				set_block(addr->ip6, 6,  embedded.ip >> 16);
				set_block(addr->ip6, 7, (unsigned short)(embedded.ip & 0xFFFF));
				break;
			}
		}

	} /* for i < 72 */
	addr->ip_ver = IPV6_A;
	return IPV6_A;
}

int string2addr(const char *s, struct ip_addr *addr, int len) {
	int i;
	int may_ipv4 = 0, may_ipv6 = 0;

	for (i = 0; i < len; i++) {
		if (s[i] == '.' && !may_ipv6) {
			may_ipv4 = 1;
			break;
		} else if (s[i] == ':') {
			may_ipv6 = 1;
			break;
		} else if (isdigit(s[i]))
			continue;
		else if (s[i] == '\0')
			break;
		else if ((s[i] >= 'a' && s[i] <= 'f') || (s[i] >= 'A' && s[i] <= 'F')) {
			may_ipv6 = 2;
			continue; /* this is valid for IPv6 only */
		} else {
			debug(PARSEIP, 3, "Invalid IP %s,  contains [%c]\n", s, s[i]);
			return BAD_IP;
		}
	}
	if (may_ipv4)
		return string2addrv4(s, addr, len);
	if (may_ipv6 == 1)
		return string2addrv6(s, addr, len);

	debug(PARSEIP, 3, "Invalid IPv4 or IPv6 : %s\n", s);
	return BAD_IP;
}

/*
 * returns :
 *    IPV4_A : IPv4 without mask
 *    IPV4_N : IPv4 + mask
 *    IPV6_A : IPv6 without mask
 *    IPV6_N : IPv6 +  mask
 *    BAD_IP, BAD_MASK on error
 */
int get_subnet_or_ip(const char *s, struct subnet *subnet) {
	int i, a;
	u32 mask;
	int count_slash = 0;
	int slash_i = 0;

	if (*s == '\0' || *s == '/') {
		debug(PARSEIP, 3, "Invalid prefix %s, null IP\n", s);
		return BAD_IP;
	}
	debug(PARSEIP, 9, "prefix %s length %d\n", s, (int)strlen(s));
	for (i = 0; i < strlen(s); i++) {
		if (s[i] == '/') {
			count_slash++;
			slash_i = i;
		} else if (isxdigit(s[i])||s[i] == '.'||s[i] == ':'||s[i] == ' ')
			continue;
		else {
			debug(PARSEIP, 3, "Invalid prefix '%s', contains '%c'\n", s, s[i]);
			return BAD_IP;
		}
	}

	if (count_slash == 0) {
		a =  string2addr(s, &subnet->ip_addr, 41);
		if (a == BAD_IP)
			return a;
		subnet->mask = (a == IPV6_A ? 128 : 32);
		return a;
	} else if (count_slash == 1) {
		debug(PARSEIP, 5, "trying to parse ip/mask %s\n", s);
		a = string2addr(s, &subnet->ip_addr, slash_i);
		if (a == BAD_IP)
			return a;
		mask = string2mask(s + slash_i + 1, 41);
		if (mask == BAD_MASK)
			return BAD_MASK;
		subnet->mask = mask;
		if (a == IPV4_A)
			return IPV4_N;
		else if (a == IPV6_A)
			return IPV6_N;
	}
	debug(PARSEIP, 3, "bad prefix '%s'\n", s);
	return BAD_IP;
}


int ipv4_get_classfull_mask(const struct subnet *s) {
	if (s->ip == 0)
		return 0;
	if ((s->ip >> 31) == 0)
		return 8;
	if ((s->ip >> 30) == 2)
		return 16;
	if ((s->ip >> 29) == 6)
		return 24;
	if ((s->ip >> 28) == 14) /* MULTICAST is /32 but hum ... */
		return 32;
	if ((s->ip >> 28) == 15)
		return -1;
	return -1;
}

/* some platforms will print IPv4 classfull subnet and remove 0 like 10/8, 172.30/16 etc
 * or sh ip bgp will not print the mask in case of a classfull subnet
 * thanks CISCO for keeping that 1980's crap into our memories ...
 * ... */
int classfull_get_subnet(const char *s, struct subnet *subnet) {
	int truc[4];
	int i;
	u32 mask = 0;
	int current_block = 0;
	int count_dot = 0;

	if  (s[0] == '.') {
		debug(PARSEIP, 3, "Invalid prefix '%s', starts with '.'\n", s);
		return BAD_IP;
	}
	memset(truc, 0, sizeof(truc));
	for (i = 0; ; i++) {
		if (s[i] == '\0' || s[i] == '/') {
			truc[count_dot] = current_block;
			if (current_block > 255) {
				debug(PARSEIP, 3, "Invalid IP '%s', %d too big\n", s, current_block);
				return BAD_IP;
			}
			break;
		} else if (s[i] == '.') {
			if  (s[i + 1] == '.') {
				debug(PARSEIP, 3, "Invalid IP '%s', contains 2 consecutives '.'\n", s);
				return BAD_IP;
			}
			if  (s[i + 1] == '\0') {
				debug(PARSEIP, 3, "Invalid IP '%s', ends with '.'\n", s);
				return BAD_IP;
			}
			if (current_block > 255) {
				debug(PARSEIP, 3, "Invalid IP '%s', %d too big\n", s, current_block);
				return BAD_IP;
			}
			if (count_dot == 3) {
				debug(PARSEIP, 3, "Invalid IP '%s', too many '.'\n", s);
				return BAD_IP;
			}
			truc[count_dot] = current_block;
			count_dot++;
			current_block = 0;
		} else if (isdigit(s[i])) {
			current_block *= 10;
			current_block += s[i] - '0';
			continue;
		} else if (s[i] == ':') {
			/* that may be IPv6 and IPv6 is classless, so fall-back to a regular get_subnet_or_ip */
			return get_subnet_or_ip(s, subnet);
		} else {
			debug(PARSEIP, 3, "Invalid IP '%s',  contains '%c'\n", s, s[i]);
			return BAD_IP;
		}
	}
	if (s[i] == '\0') {
		subnet->ip = (truc[0] << 24) + (truc[1] << 16) + (truc[2] << 8) + truc[3];
		subnet->ip_ver = IPV4_A;
		subnet->mask = ipv4_get_classfull_mask(subnet);
		return IPV4_N;
	}
	if (s[i] != '/') {
		debug(PARSEIP, 3, "Invalid classfull prefix '%s', no mask\n", s);
		return BAD_IP;
	}
	i++;
	if (s[i] == '\0') {
		debug(PARSEIP, 3, "Invalid classfull prefix '%s', no mask\n", s);
		return BAD_IP;
	}
	for (; ; i++) {
		if (s[i] == '\0')
			break;
		if (!isdigit(s[i])) {
			debug(PARSEIP, 3, "Invalid prefix '%s', mask contains '%c'\n", s, s[i]);
			return BAD_IP;
		}
		mask *= 10;
		mask += s[i] - '0';
	}
	if (mask > 32) {
		debug(PARSEIP, 3, "Invalid prefix '%s', mask '%d' too big \n", s, (int)mask);
		return BAD_MASK;
	}
	subnet->ip = (truc[0] << 24) + (truc[1] << 16) + (truc[2] << 8) + truc[3];
        subnet->ip_ver = IPV4_A;
	subnet->mask = mask;
	return IPV4_N;
}

int addr_is_superior(const struct ip_addr *ip1, const struct ip_addr *ip2) {
	int i, res;

	if (ip1->ip_ver != ip2->ip_ver) {
		debug(ADDRCOMP, 1, "cannot compare, different IP version %d != %d\n", ip1->ip_ver, ip2->ip_ver);
		return -1;
	}
	res = 0;
	if (ip1->ip_ver == IPV4_A)
		return  (ip1->ip < ip2->ip);

	if (ip1->ip_ver == IPV6_A) {
		for (i = 0; i < 8; i++) {
			if (block(ip1->ip6, i) < block(ip2->ip6, i)) {
				res = 1;
				break;
			} else if (block(ip1->ip6, i) > block(ip2->ip6, i)) {
				res = 0;
				break;
			}
		}
		return res;
	}
	return -1;
}

int subnet_is_superior(const struct subnet *s1, const struct subnet *s2) {
	int i, res;

	if (s1->ip_ver != s2->ip_ver) {
		debug(ADDRCOMP, 1, "cannot compare, different IP version\n");
		return -1;
	}
	res = 0;
	if (s1->ip_ver == IPV4_A) {
		if (s1->ip == s2->ip) {
			if (s1->mask < s2->mask)
				res = 1;
			else
 				res = 0;
		} else if  (s1->ip < s2->ip)
			res =  1;
		else
			res =  0;
		st_debug(ADDRCOMP, 5, "%P %c %P\n", *s1, (res ? '>' : '<' ), *s2);
		return res;
	}

	if (s1->ip_ver == IPV6_A) {
		if (is_equal_ipv6(s1->ip6, s2->ip6)) {
			if (s1->mask < s2->mask)
				res = 1;
			else
				res = 0;
		} else {
			for (i = 0; i < 8; i++) {
				if (block(s1->ip6, i) < block(s2->ip6, i)) {
					res = 1;
					break;
				} else if (block(s1->ip6, i) > block(s2->ip6, i)) {
					res = 0;
					break;
				}
			}
		}
		st_debug(ADDRCOMP, 5, "%P %c %P\n", *s1, (res ? '>' : '<' ), *s2);
		return res;
	}
	debug(ADDRCOMP, 1, "Invalid comparison ipver %d\n", s1->ip_ver);
	return -1;
}

/* try to aggregate s1 & s2, putting the result in s3 if possible
 * returns negative if impossible to aggregate, positive if possible */
static int aggregate_subnet_ipv4(const struct subnet *s1, const struct subnet *s2, struct subnet *s3) {
	ipv4 a, b;

	if (s1->mask != s2->mask) {
		st_debug(AGGREGATE, 5, "different masks for %P and %P, can't aggregate\n", *s1, *s2);
		return -1;
	}
	a = s1->ip >> (32 - s1->mask);
	b = s2->ip >> (32 - s1->mask);
	if (a == b) {
		st_debug(AGGREGATE, 6, "same subnet %P\n", *s1);
		s3->ip_ver = IPV4_A;
		s3->ip     = s1->ip;
		s3->mask   = s1->mask;
		return 1;
	}
	if (a >> 1 != b >> 1) {
		st_debug(AGGREGATE, 5, "cannot aggregate %P and %P\n", *s1, *s2);
		return -1;
	}
	s3->ip_ver = IPV4_A;
	s3->mask = s1->mask - 1;
	s3->ip   = (a >> 1) << (32 - s3->mask);
	st_debug(AGGREGATE, 5, "can aggregate %P and %P into : %P\n", *s1, *s2, *s3);
	return 1;
}

static int aggregate_subnet_ipv6(const struct subnet *s1, const struct subnet *s2, struct subnet *s3) {
	ipv6 a, b;

	if (s1->mask != s2->mask) {
		st_debug(AGGREGATE, 5, "different masks for %P and %P, can't aggregate\n", *s1, *s2);
		return -1;
	}
	memcpy(&a, &s1->ip6, sizeof(a));
	memcpy(&b, &s2->ip6, sizeof(a));
	shift_ipv6_right(a, (128 - s1->mask));
	shift_ipv6_right(b, (128 - s1->mask));
	if (is_equal_ipv6(a, b)) {
		st_debug(AGGREGATE, 5, "same subnet %P\n", *s1);
		copy_subnet(s3, s1);
		return 1;
	}
	shift_ipv6_right(a, 1);
	shift_ipv6_right(b, 1);
	if (!is_equal_ipv6(a, b)) {
		st_debug(AGGREGATE, 5, "cannot aggregate %P and %P\n", *s1, *s2);
		return -1;
	}
	s3->mask = s1->mask - 1;
	shift_ipv6_left(a, 128 - s3->mask);
	s3->ip_ver = IPV6_A;
	memcpy(&s3->ip6, &a, sizeof(a));
	st_debug(AGGREGATE, 5, "can aggregate %P and %P into : %P\n", *s1, *s2, *s3);
	return 1;
}

int aggregate_subnet(const struct subnet *s1, const struct subnet *s2, struct subnet *s3) {
	if (s1->ip_ver != s2->ip_ver)
		return -4;

	if (s1->ip_ver == IPV4_A)
		return aggregate_subnet_ipv4(s1, s2, s3);
	if (s1->ip_ver == IPV6_A)
		return aggregate_subnet_ipv6(s1, s2, s3);
	return -3;
}

void first_ip(struct subnet *s) {
	if (s->ip_ver == IPV4_A) {
		s->ip >>= (32 - s->mask);
		s->ip <<= (32 - s->mask);
	} else if (s->ip_ver == IPV6_A) {
		shift_ipv6_right(s->ip6, 128 - s->mask);
		shift_ipv6_left(s->ip6, 128 -s->mask);
	}
}

void last_ip(struct subnet *s) {
	int i, j;

	if (s->ip_ver == IPV4_A) {
		for (i = 0; i < (32 - s->mask); i++)
			s->ip |= (1 << i);
	} else if (s->ip_ver == IPV6_A) { /* do you love binary math? yes you do! */
		for (i = 0; i < (128 - s->mask) / 16; i++)
			set_block(s->ip6, 7 - i, 0xFFFF);
		for (j = 0; j < ((128 - s->mask) % 16); j++)
			block_OR(s->ip6, 7 - i,  (1 << j));
	}
}

void next_subnet(struct subnet *s) {
	if (s->ip_ver == IPV4_A) {
		s->ip >>= (32 - s->mask);
		s->ip += 1;
		s->ip <<= (32 - s->mask);
	} else if (s->ip_ver == IPV6_A) {
		shift_ipv6_right(s->ip6, 128 - s->mask);
		increase_ipv6(s->ip6);
		shift_ipv6_left(s->ip6, 128 - s->mask);
	}
}

void previous_subnet(struct subnet *s) {
	if (s->ip_ver == IPV4_A) {
		s->ip >>= (32 - s->mask);
		s->ip -= 1;
		s->ip <<= (32 - s->mask);
	} else if (s->ip_ver == IPV6_A) {
		shift_ipv6_right(s->ip6, 128 - s->mask);
		decrease_ipv6(s->ip6);
		shift_ipv6_left(s->ip6, 128 - s->mask);
	}
}

int can_decrease_mask(const struct subnet *s) {
	ipv4 a;
	ipv6 b;
	int i = 0;

	if (s->ip_ver == IPV4_A) {
		a = (s->ip >> (32 - s->mask));
		while (i < s->mask) {
			if (a & 1)
				break;
			i++;
			a >>= 1;
		}
		return i;
	}
	if (s->ip_ver == IPV6_A) {
		memcpy(&b, &s->ip6, sizeof(ipv6));
		shift_ipv6_right(b, 128 - s->mask);
		while (i < s->mask) {
			if (block(b, 7) & 1)
				break;
			i++;
			shift_ipv6_right(b, 1);
		}
		return i;
	}
	return 0;
}

/*
 * remove s2 from s1 if possible
 * alloc a new struct subnet *
 * number of element is stored in *n
 */
struct subnet *subnet_remove(const struct subnet *s1, const struct subnet *s2, int *n) {
	int res;
	struct subnet *news;
	struct subnet test;
	int a, i;

	res = subnet_compare(s1, s2);
	if (res == EQUALS) {
		st_debug(ADDRREMOVE, 5, "same prefix %P\n", *s1);
		*n = 0;
		return NULL;
	} else if (res == NOMATCH || res == INCLUDED) {
		news = malloc(sizeof(struct subnet));
		if (news == NULL) {
			*n = -1;
			return NULL;
		}
		st_debug(ADDRREMOVE, 5, "%P is not included in %P\n", *s2, *s1);
		copy_subnet(news, s1);
		*n = 1;
		return news;
	}
	/* s2 in included in s1 */
	res = s2->mask - s1->mask;
	news = malloc(sizeof(struct subnet) * res);
	if (news == NULL) {
		*n = -1;
		return NULL;
	}
	copy_subnet(&test, s1);
	first_ip(&test);
	i = 0;
	/** strategy is as follow :
	test = S1;
	do
		do
			increase test mask
		until test doesn't include S2 any more

		test = test + 1 (next subnet)
	until test != S2

	test = S2
	do
		test = test +1 (next subnet)
		decrease test mask as much as we can (aggregation)
	until test included in S1
	  */
	while (1) { /* getting subnet before s2*/
		a = 0;
		while (1) {
			test.mask += 1;
			st_debug(ADDRREMOVE, 3, "Loop#1 testing %P\n", test);
			res = subnet_compare(&test, s2);
			if (res == INCLUDES) {
				st_debug(ADDRREMOVE, 5, "Loop#1 %P too BIG, includes %P, continuing loop\n", test, *s2);
				continue;
			} else if (res == NOMATCH) {
				st_debug(ADDRREMOVE, 5, "Loop#1 %P BIG enough, breaking loop\n", test);
				copy_subnet(&news[i], &test);
				i++;
				break;
			} else if (res == EQUALS) {
				st_debug(ADDRREMOVE, 5, "Loop#1 %P EQUALS, breaking final loop\n", test);
				a++;
				break;
			}
		}
		if (a) {
			break;
		} else {
			next_subnet(&test);
			st_debug(ADDRREMOVE, 5, "Loop#1 advancing to %P\n", test);
			res = subnet_compare(&test, s2);
			if (res == EQUALS) {
				st_debug(ADDRREMOVE, 5, "Loop#1 finally we reached %P\n", *s2);
				break;
			}
		}
	}
	/* getting subnet after s2 */
	copy_subnet(&test, s2);

	while (1) {
		next_subnet(&test);
		st_debug(ADDRREMOVE, 5, "Loop#2 testing %P\n", test);
		a = can_decrease_mask(&test);
		test.mask -= a;
		st_debug(ADDRREMOVE, 5, "Loop#2 increased to %P\n", test);
		res = subnet_compare(&test, s1);
		if (res == NOMATCH) {
			st_debug(ADDRREMOVE, 5, "Loop#2 finally %P bigger than %P\n", test, *s1);
			break;
		}
		copy_subnet(&news[i], &test);
		i++;
	}
	*n = i;
	return news;
}
