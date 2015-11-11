/*
 *  string to IPv4, IPv6 functions
 *  these a meant to be crazy fast so code is ... fun
 *
 * Copyright (C) 2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "st_options.h"
#include "debug.h"
#include "utils.h"
#include "string2ip.h"

sprint_hex(short)
sprint_unsigned(int)

int string2mask(const char *s, size_t len)
{
	int ddn_mask = 0;
	int a, prev_a = 0;
	int count_dot = 0;
	const char *s_max = s + len;
#ifdef DEBUG_PARSE_IPV4
	const char *p = s;
#endif

	/* masks must begin with a digit */
	if (!isdigit(*s)) {
		debug_parseipv4(3, "Invalid mask '%s', starts with '.'\n", p);
		return BAD_MASK;
	}
	/* since we are sure it is a digit, avoid a loop iteration */
	a = *s - '0';
	s++;

	while (1) {
		if (*s == '\0' || s == s_max) {
			if (count_dot == 0) {/* prefix-notation mask */
				if (a > 128) {
					debug_parseipv4(3, "Invalid mask '%s', too long\n", p);
					return BAD_MASK;
				}
				return a;
			} else if (count_dot != 3) {
				debug_parseipv4(3, "Invalid DDN mask '%s', not enough '.'\n", p);
				return BAD_MASK;
			}
			if (a > 255) {
				debug_parseipv4(3,
						"Invalid DDN mask, contains '%d' > 255\n", a);
				return BAD_MASK;
			}
			if (!isPower2(256 - a)) {
				debug_parseipv4(3,
						"Invalid DDN mask, 256 - '%d' is not power of 2\n",
						a);
				return BAD_MASK;
			}
			if (a > prev_a) {
				debug_parseipv4(3, "Invalid DDN mask, '%d' > %d\n", a, prev_a);
				return BAD_MASK;
			}
			if (a && (a < 255) && (prev_a != 255)) { /* 255.240.224.0 */
				debug_parseipv4(3, "Invalid DDN mask, wrong\n");
				return BAD_MASK;
			}
			ddn_mask += 8 - mylog2(256 - a);
			break;
		} else if (*s == '.') {
			s++;
			if (*s == '.') {
				debug_parseipv4(3, "Invalid DDN mask '%s', consecutive '.'\n", p);
				return BAD_MASK;
			}
			if (*s == '\0' || s == s_max) {
				debug_parseipv4(3, "Invalid DDN mask '%s', ends with '.'\n", p);
				return BAD_MASK;
			}
			if (count_dot == 3) {
				debug_parseipv4(3, "Invalid DDN mask '%s', too many '.'\n", p);
				return BAD_MASK;
			}
			if (a > 255) {
				debug_parseipv4(3, "Invalid DDN mask, contains '%d' > 255\n", a);
				return BAD_MASK;
			}
			if (!isPower2(256 - a)) {
				debug_parseipv4(3,
						"Invalid DDN mask, 256 - '%d' is not power of 2\n",
						a);
				return BAD_MASK;
			}
			if (count_dot) {
				if (a > prev_a) {
					debug_parseipv4(3, "Invalid DDN mask, '%d' > %d\n",
							a, prev_a);
					return BAD_MASK;
				}
				if (a && (a < 255) && (prev_a != 255)) { /* 255.240.224.0 */
					debug_parseipv4(3, "Invalid DDN mask, wrong\n");
					return BAD_MASK;
				}
			}
			prev_a = a;
			ddn_mask += 8 - mylog2(256 - a);
			count_dot++;
			a = 0;
		} else if (isdigit(*s)) {
			a *= 10;
			a += *s - '0';
			s++;
			continue;
		} else {
			debug_parseipv4(3, "Invalid mask '%s', contains '%c'\n", p, *s);
			return BAD_MASK;
		}
	}
	return ddn_mask;
}

/* can only be called from string2addr, which make some preliminary tests */
static int string2addrv4(const char *s, struct ip_addr *addr, size_t len)
{
	int count_dot = 0;
	int truc[4];
	int current_block = 0;
	const char *s_max;
	int num_digit = 0;

	/* string2addr has made sure s[0] is not NUL or '.' */
	s_max = s + len;
	while (1) {
		if (s == s_max)
			goto out_ipv4;
		switch (*s) {
		case '.':
			s++;
			if (current_block > 255)
				return BAD_IP;
			if (count_dot == 3)
				return BAD_IP;
			if  (s == s_max)
				return BAD_IP;
			if (!isdigit(*s))
				return BAD_IP;
			truc[count_dot] = current_block;
			count_dot++;
			current_block = *s - '0';
			num_digit = 1;
			s++;
			continue;
		/* crazy isdigit(*s) */
		case '0':
			if (num_digit++ == 3)
				return BAD_IP;
			current_block *= 10;
			s++;
			continue;
		case '1':
			if (num_digit++ == 3)
				return BAD_IP;
			current_block *= 10;
			current_block += 1;
			s++;
			continue;
		case '2':
			if (num_digit++ == 3)
				return BAD_IP;
			current_block *= 10;
			current_block += 2;
			s++;
			continue;
		case '3':
			if (num_digit++ == 3)
				return BAD_IP;
			current_block *= 10;
			current_block += 3;
			s++;
			continue;
		case '4':
			if (num_digit++ == 3)
				return BAD_IP;
			current_block *= 10;
			current_block += 4;
			s++;
			continue;
		case '5':
			if (num_digit++ == 3)
				return BAD_IP;
			current_block *= 10;
			current_block += 5;
			s++;
			continue;
		case '6':
			if (num_digit++ == 3)
				return BAD_IP;
			current_block *= 10;
			current_block += 6;
			s++;
			continue;
		case '7':
			if (num_digit++ == 3)
				return BAD_IP;
			current_block *= 10;
			current_block += 7;
			s++;
			continue;
		case '8':
			if (num_digit++ == 3)
				return BAD_IP;
			current_block *= 10;
			current_block += 8;
			s++;
			continue;
		case '9':
			if (num_digit++ == 3)
				return BAD_IP;
			current_block *= 10;
			current_block += 9;
			s++;
			continue;
		case '\0':
			goto out_ipv4;
		default:
			return BAD_IP;
		}
	}
out_ipv4:
	if (count_dot != 3 || current_block > 255)
		return BAD_IP;
	addr->ip = (truc[0] << 24) + (truc[1] << 16) + (truc[2] << 8) + current_block;
	addr->ip_ver = IPV4_A;
	return IPV4_A;
}

static int string2addrv6(const char *s, struct ip_addr *addr, size_t len)
{
	int j, k;
	int out_i = 0, out_i2, num_digit = 0;
	unsigned short current_block = 0;
	unsigned short block_right[8];
	struct ip_addr embedded;
	const char *s_max = s + len;
	const char *p = s;

	/* first loop, before '::' if any */
	while (1) {
		if (s == s_max) {
			if (out_i != 7) {
				debug(PARSEIPV6, 3, "Invalid IPv6 '%s', only %d blocks\n",
						p, out_i);
				return BAD_IP;
			}
			set_block(addr->ip6, out_i, current_block);
			addr->ip_ver = IPV6_A;
			return IPV6_A;
		}
		switch (*s) {
		case ':':
			if (out_i >= 7) {
				debug(PARSEIPV6, 3, "Invalid IPv6 '%s',too many blocks\n", p);
				return BAD_IP;
			}
			set_block(addr->ip6, out_i, current_block);
			out_i++;
			current_block = 0;
			num_digit = 0;
			/* compressed */
			s++;
			if (*s == ':') {
				s++;
				if (*s == ':') {
					debug(PARSEIPV6, 1, "Invalid IPv6 '%s' :::\n", s);
					return BAD_IP;
				}
				goto loop2;
			}
			continue;
		case '0':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			num_digit++;
			s++;
			continue;
		case '1':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 1;
			num_digit++;
			s++;
			continue;
		case '2':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 2;
			num_digit++;
			s++;
			continue;
		case '3':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 3;
			num_digit++;
			s++;
			continue;
		case '4':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 4;
			num_digit++;
			s++;
			continue;
		case '5':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 5;
			num_digit++;
			s++;
			continue;
		case '6':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 6;
			num_digit++;
			s++;
			continue;
		case '7':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 7;
			num_digit++;
			s++;
			continue;
		case '8':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 8;
			num_digit++;
			s++;
			continue;
		case '9':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 9;
			num_digit++;
			s++;
			continue;
		case 'a':
		case 'A':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 10;
			num_digit++;
			s++;
			continue;
		case 'b':
		case 'B':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 11;
			num_digit++;
			s++;
			continue;
		case 'c':
		case 'C':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 12;
			num_digit++;
			s++;
			continue;
		case 'd':
		case 'D':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 13;
			num_digit++;
			s++;
			continue;
		case 'e':
		case 'E':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 14;
			num_digit++;
			s++;
			continue;
		case 'f':
		case 'F':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 15;
			num_digit++;
			s++;
			continue;
		case '.':
			/** embedded IPv4 address, MAYBE? */
			s -= num_digit;
			debug_parseipv6(8, "String '%s' MAYBE embedded IPv4\n", s);
			if (out_i != 6) {
				debug(PARSEIPV6, 3,
						"Invalid IPv6 '%s', need 6 blocks before IPv4\n",
						p);
				return BAD_IP;
			}
			j = string2addrv4(s, &embedded, s_max - s);
			if (j < 0)
				return j;
			set_block(addr->ip6, 6,  embedded.ip >> 16);
			set_block(addr->ip6, 7, (unsigned short)(embedded.ip & 0xFFFF));
			addr->ip_ver = IPV6_A;
			return IPV6_A;
		case '\0':
			if (out_i != 7) {
				debug(PARSEIPV6, 3, "Invalid IPv6 '%s', only %d blocks\n",
						p, out_i);
				return BAD_IP;
			}
			set_block(addr->ip6, out_i, current_block);
			addr->ip_ver = IPV6_A;
			return IPV6_A;
		default:
			debug(PARSEIPV6, 3, "Invalid char '%c' found in block#%d\n", *p, out_i);
			return BAD_IP;
		}
	}
loop2:
	out_i2 = 0;
	/* second loop, trying to get the right part after '::' */
	while (1) {
		if (s == s_max) {
			block_right[out_i2] = current_block;
			out_i2++;
			goto end_ipv6;
		}
		switch (*s) {
		case ':':
			if (out_i + out_i2 >= 6) {
				debug(PARSEIPV6, 3, "Invalid IPv6 '%s',too many blocks\n", p);
				return BAD_IP;
			}
			s++;
			if (*s == ':') {
				debug(PARSEIPV6, 3, "Invalid IPv6 '%s', two '::'\n", p);
				return BAD_IP;
			}
			block_right[out_i2] = current_block;
			out_i2++;
			current_block = 0;
			num_digit = 0;
			continue;
		case '0':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			num_digit++;
			s++;
			continue;
		case '1':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block++;
			num_digit++;
			s++;
			continue;
		case '2':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 2;
			num_digit++;
			s++;
			continue;
		case '3':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 3;
			num_digit++;
			s++;
			continue;
		case '4':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 4;
			num_digit++;
			s++;
			continue;
		case '5':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 5;
			num_digit++;
			s++;
			continue;
		case '6':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 6;
			num_digit++;
			s++;
			continue;
		case '7':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 7;
			num_digit++;
			s++;
			continue;
		case '8':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 8;
			num_digit++;
			s++;
			continue;
		case '9':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 9;
			num_digit++;
			s++;
			continue;
		case 'a':
		case 'A':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 10;
			num_digit++;
			s++;
			continue;
		case 'b':
		case 'B':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 11;
			num_digit++;
			s++;
			continue;
		case 'c':
		case 'C':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 12;
			num_digit++;
			s++;
			continue;
		case 'd':
		case 'D':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 13;
			num_digit++;
			s++;
			continue;
		case 'e':
		case 'E':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 14;
			num_digit++;
			s++;
			continue;
		case 'f':
		case 'F':
			if (num_digit == 4)
				return BAD_IP;
			current_block <<= 4;
			current_block += 15;
			num_digit++;
			s++;
			continue;
		case '.':
			s -= num_digit;
			debug_parseipv6(8, "String '%s' MAYBE embedded IPv4\n", s);
			if (out_i + out_i2 > 5) {
				debug(PARSEIPV6, 3,
						"Invalid IPv6 '%s', need 6 blocks before IPv4\n",
						s);
				return IPV4_A;
			}
			j = string2addrv4(s, &embedded, s_max - s);
			if (j < 0)
				return j;
			set_block(addr->ip6, 6,  embedded.ip >> 16);
			set_block(addr->ip6, 7, (unsigned short)(embedded.ip & 0xFFFF));
			addr->ip_ver = IPV6_A;
			for (j = out_i; j < 6 - out_i2; j++) {
				set_block(addr->ip6, j, 0);
				debug_parseipv6(8, "copying '%x' to block#%d\n", 0, j);
			}
			for (k = 0 ; k < out_i2; k++, j++) {
				debug_parseipv6(8, "copying '%x' to block#%d\n", block_right[k], j);
				set_block(addr->ip6, j, block_right[k]);
			}
			return IPV6_A;
		case '\0':
			block_right[out_i2] = current_block;
			out_i2++;
			goto end_ipv6;
		default:
			debug(PARSEIPV6, 3, "Invalid char '%c' found in block#%d\n", *s, out_i);
			return BAD_IP;
		}
	}
end_ipv6:
	for (j = out_i; j < 8 - out_i2; j++)
		set_block(addr->ip6, j, 0);
	for (k = 0 ; k < out_i2; k++, j++)
		set_block(addr->ip6, j, block_right[k]);
	addr->ip_ver = IPV6_A;
	return IPV6_A;
}

int string2addr(const char *s, struct ip_addr *addr, size_t len)
{
	const char *p = s;

	/* check first char */
	if (*p == '.')
		return BAD_IP;
	if (*p == ':') {
		if (p[1] == ':')
			return string2addrv6(s, addr, len);
		return BAD_IP;
	}
	if (!isxdigit(*p))
		return BAD_IP;
	p++;
#define TEST_IPVER_BLOCK \
	do { \
		if (!isdigit(*p)) { \
			if (*p == '.') \
			return string2addrv4(s, addr, len); \
			if (*p == ':') \
			return string2addrv6(s, addr, len); \
			if ((*p >= 'a' && *p <= 'f') || (*p >= 'A' || *p <= 'F')) \
			return string2addrv6(s, addr, len); \
			return BAD_IP; \
		} \
		p++; \
	} while (0)
	TEST_IPVER_BLOCK;
	TEST_IPVER_BLOCK;
	TEST_IPVER_BLOCK;
	TEST_IPVER_BLOCK;
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
int get_subnet_or_ip(const char *s, struct subnet *subnet)
{
	int a;
	u32 mask;
	char *p;

	if (*s == '\0' || *s == '/') {
		debug(PARSEIP, 3, "Invalid prefix %s, null IP\n", s);
		return BAD_IP;
	}
	p = strchr(s, '/');
	if (p == NULL) {
		a =  string2addr(s, &subnet->ip_addr, 41);
		if (a == BAD_IP)
			return a;
		subnet->mask = (a == IPV6_A ? 128 : 32);
		return a;
	}
	debug_parseipv4(8, "trying to parse ip/mask %s\n", s);
	a = string2addr(s, &subnet->ip_addr, p - s);
	if (a == BAD_IP)
		return a;
	mask = string2mask(p + 1, 41);
	if (mask == BAD_MASK)
		return BAD_MASK;
	subnet->mask = mask;
	if (a == IPV4_A)
		return IPV4_N;
	else if (a == IPV6_A)
		return IPV6_N;
	else
		return BAD_IP;
}

/* some platforms will print IPv4 classfull subnet and remove 0 like 10/8, 172.30/16 etc
 * or sh ip bgp will not print the mask in case of a classfull subnet
 * thanks CISCO for keeping that 1980's crap into our memories ...
 */
int classfull_get_subnet(const char *s, struct subnet *subnet)
{
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
				debug(PARSEIP, 3, "Invalid IP '%s', %d too big\n",
						s, current_block);
				return BAD_IP;
			}
			break;
		} else if (s[i] == '.') {
			if  (s[i + 1] == '.') {
				debug(PARSEIP, 3, "Invalid IP '%s', 2 consecutives '.'\n", s);
				return BAD_IP;
			}
			if  (s[i + 1] == '\0') {
				debug(PARSEIP, 3, "Invalid IP '%s', ends with '.'\n", s);
				return BAD_IP;
			}
			if (current_block > 255) {
				debug(PARSEIP, 3, "Invalid IP '%s', %d too big\n",
						s, current_block);
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
			/* that may be IPv6 and IPv6 is classless,
			 * so fall-back to a regular get_subnet_or_ip
			 */
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
		debug(PARSEIP, 3, "Invalid prefix '%s', mask '%d' too big\n", s, (int)mask);
		return BAD_MASK;
	}
	subnet->ip = (truc[0] << 24) + (truc[1] << 16) + (truc[2] << 8) + truc[3];
	subnet->ip_ver = IPV4_A;
	subnet->mask = mask;
	return IPV4_N;
}
