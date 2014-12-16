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
#include "bitmap.h"
#include "utils.h"
#include "heap.h"

static inline int is_valid_ip_char(char c) {
	return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}


void fprint_route(const struct route *r, FILE *output, int compress_level) {
	char buffer[52];
	char buffer2[52];
	struct subnet s;

	subnet2str(&r->subnet, buffer, compress_level);
	memcpy(&s.ip6, &r->gw, 16);
	s.ip_ver = r->subnet.ip_ver;
	subnet2str(&s, buffer2, 2);
	fprintf(output, "%s;%d;%s;%s;%s\n", buffer, r->subnet.mask, r->device, buffer2, r->comment);
}

void fprint_route_fmt(const struct route *r, FILE *output, const char *fmt) {
	int i, j, a ,i2, compression_level;
	char c;
	char outbuf[512 + 140];
	char buffer[128], temp[32];
	struct subnet sub;
	char BUFFER_FMT[32];
	int field_width;
/* %I for IP */
/* %m for mask */
/* %D for device */
/* %g for gateway */
/* %C for comment */
	i = 0;
	j = 0; /* index int outbuf */
	while (1) {
		c = fmt[i];
		debug(FMT, 5, "Still to parse : '%s'\n", fmt + i);
		if ( j > sizeof(outbuf) - 140) { /* 128 is the max size (comment) */
			debug(FMT, 1, "output buffer maybe too small, aborting\n");
			break;
		}
		if (c == '\0')
			break;
		if (c == '%') {
			i2 = i + 1;
			a = 0;
			/* try to get a field width (if any) */
			if (fmt[i2] == '\0') {
				debug(FMT, 2, "End of String after a %c\n", '%');
				outbuf[j++] = '%';
				break;
			} else if (fmt[i2] == '-') {
				temp[0] = '-';
				i2++;
			}
			/*
			 * try to get an int into temp[]
			 * temp should hold the size of the ouput buf
			 */
			while (isdigit(fmt[i2])) {
				temp[i2 - i - 1] = fmt[i2];
				i2++;
			}
			temp[i2 - i - 1] = '\0';
			debug(FMT, 9, "Field width String  is '%s'\n", temp);
			if (temp[0] == '-' && temp[1] == '\0') {
				debug(FMT, 2, "Invalid field width '-'");
				field_width = 0;

			} else if (temp[0] == '\0') {
				field_width = 0;
			} else {
				field_width = atoi(temp);
			}
			debug(FMT, 9, "Field width Int is '%d'\n", field_width);
			i += strlen(temp);
			/* preparing FMT */
			if (field_width)
				sprintf(BUFFER_FMT, "%%%ds", field_width);
			else
				sprintf(BUFFER_FMT, "%%s");
			switch (fmt[i2]) {
				case '\0':
					outbuf[j] = '%';
					debug(FMT, 2, "End of String after a %c\n", '%');
					a += 1;
					i--;
					break;
				case 'M':
					if (r->subnet.ip_ver == IPV4_A)
						mask2ddn(r->subnet.mask, buffer);
					else
						sprintf(buffer, "%d", r->subnet.mask);
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'm':
					sprintf(buffer, "%d", r->subnet.mask);
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'D':
					a += sprintf(outbuf + j, BUFFER_FMT, r->device);
					break;
				case 'C':
					a += sprintf(outbuf + j, BUFFER_FMT, r->comment);
					break;
				case 'I':
					if (fmt[i2 + 1] == '0' || fmt[i2 + 1] == '1' || fmt[i2 + 1] == '2') {
						compression_level = fmt[i2 + 1] - '0';
						i++;
					} else
						 compression_level = 2;
					subnet2str(&r->subnet, buffer, compression_level);
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'G':
					if (fmt[i2 + 1] == '0' || fmt[i2 + 1] == '1' || fmt[i2 + 1] == '2') {
						compression_level = fmt[i2 + 1] - '0';
						i++;
					} else
						 compression_level = 2;
					memcpy(&sub.ip6, &r->gw6, sizeof(ipv6));
					sub.ip_ver = r->subnet.ip_ver;
					subnet2str(&sub, buffer, compression_level);
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				default:
					debug(FMT, 2, "%c is not a valid char after a %c\n", fmt[i2], '%');
					outbuf[j] = '%';
					outbuf[j + 1] = fmt[i2];
					a = 2;
			} //switch
			j += a;
			i += 2;
		} else if (c == '\\') {
			switch (fmt[i + 1]) {
			case '\0':
				outbuf[j++] = '\\';
				debug(FMT, 2, "End of String after a %c\n", '\\');
				a = 1;
				i--;
				break;
			case 'n':
				outbuf[j++] = '\n';
				break;
			case 't':
				outbuf[j++] = '\t';
				break;
			case ' ':
				outbuf[j++] = ' ';
				break;
			default:
				debug(FMT, 2, "%c is not a valid char after a %c\n", fmt[i + 1], '\\');
				outbuf[j++] = fmt[i + 1];
			}
			i += 2;
		} else {
			outbuf[j++] = c;
			i++;
		}
	}
	outbuf[j] = '\0';
	fprintf(output, "%s\n", outbuf);
}

void fprint_subnet_file(const struct subnet_file *sf, FILE *output, int compress_level) {
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		fprint_route(&sf->routes[i], output, compress_level);
}

void fprint_subnet_file_fmt(const struct subnet_file *sf, FILE *output, const char *fmt) {
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		fprint_route_fmt(&sf->routes[i], output, fmt);
}
void print_subnet_file(const struct subnet_file *sf, int compress_level) {
	fprint_subnet_file(sf, stdout, compress_level);
}
#define SIZE_T_MAX ((size_t)0 - 1)
int alloc_subnet_file(struct subnet_file *sf, unsigned long n) {
	if (n > SIZE_T_MAX / sizeof(struct route)) { /* being paranoid */
                fprintf(stderr, "error: too much memory requested for struct route\n");
                return -1;
	}
	sf->routes = malloc(sizeof(struct route) * n);
        debug(MEMORY, 2, "trying to alloc %lu bytes\n",  sizeof(struct route) * n);
        if (sf->routes == NULL) {
                fprintf(stderr, "error: cannot alloc  memory for sf->routes\n");
                return -1;
        }
        sf->nr = 0;
        sf->max_nr = n;
	return 0;
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
	char buffer1[51];
	if (sub1->ip_ver != sub2->ip_ver) {
		debug(ADDRCOMP, 2, "different address FAMILY\n");
		return -1;
	}
	if (sub1->ip_ver == IPV4_A)
		return subnet_compare_ipv4(sub1->ip, sub1->mask, sub2->ip, sub2->mask);
	else if (sub1->ip_ver == IPV6_A)
		return subnet_compare_ipv6(sub1->ip6, sub1->mask, sub2->ip6, sub2->mask);
	subnet2str(sub1, buffer1, 2);
	debug(ADDRCOMP, 1, "Impossible to get here, %s version = %d BUG?\n", buffer1, sub1->ip_ver);
	return -1;
}

int is_equal_ipv6(ipv6 ip1, ipv6 ip2) {
	int i;
	for (i = 0; i < 4; i++)
		if (ip1.n32[i] != ip2.n32[i])
			return 0;
	return 1;
}


int is_equal_gw(struct route *r1, struct route *r2) {
	if (r1->subnet.ip_ver != r2->subnet.ip_ver)
		return 0;
	if (r1->subnet.ip_ver == IPV4_A && r1->gw == r2->gw)
		return 1;
	else if (r1->subnet.ip_ver == IPV6_A) {
		if (!is_equal_ipv6(r1->gw6, r2->gw6))
			return 0;
		if (!is_link_local(r1->gw6))
			return 1;
		/* if link local adress, we must check if the device is the same */
		return !strcmp(r1->device, r2->device);
	}
	return 0;
}

int is_link_local(ipv6 a) {
	unsigned short x = a.n16[0];
/* link_local address is FE80::/10 */
	return (x >> 6 == 0xFE80 >> 6);
}

int subnet_compare_ipv6(ipv6 ip1, u32 mask1, ipv6 ip2, u32 mask2) {
	if (mask1 > mask2) {
		shift_right(ip1.n16, 8, (128 - mask2));
		shift_right(ip2.n16, 8, (128 - mask2));
		/*print_bitmap(ip1.n16, 8);
		print_bitmap(ip2.n16, 8);	*/
		if (is_equal_ipv6(ip1, ip2))
			return INCLUDED;
		else
			return -1;
        } else if (mask1 < mask2) {
		shift_right(ip1.n16, 8, (128 - mask1));
		shift_right(ip2.n16, 8, (128 - mask1));
		/*print_bitmap(ip1.n16, 8);
		print_bitmap(ip2.n16, 8); */
                if (is_equal_ipv6(ip1, ip2))
                        return INCLUDES;
                else
                        return -1;
        } else {
		shift_right(ip1.n16, 8, (128 - mask1));
		shift_right(ip2.n16, 8, (128 - mask1));
                if (is_equal_ipv6(ip1, ip2))
                        return EQUALS;
                else
                        return -1;
        }
	return -1;
}

int subnet_compare_ipv4(ipv4 prefix1, u32 mask1, ipv4 prefix2, u32 mask2) {
	ipv4 a, b;

	if (mask1 > mask2) {
		a = prefix1 >> (32 - mask2);
		b = prefix2 >> (32 - mask2);
		if (a == b)
			return INCLUDED;
		else
			return -1;
	} else if  (mask1 < mask2) {
		a = prefix1 >> (32 - mask1);
		b = prefix2 >> (32 - mask1);
		if (a == b)
			return INCLUDES;
		else
			return -1;
	} else {
		a = prefix1 >> (32 - mask1);
		b = prefix2 >> (32 - mask1);
		if (a == b)
			return EQUALS;
		else
			return -1;
	}
}


int addrv42str(ipv4 z, char *out_buffer) {
	int a, b, c, d;

	d = z % 256;
	c = (z >> 8) % 256;
	b = (z >> 16) % 256;
	a = (z >> 24) % 256;
	sprintf(out_buffer, "%d.%d.%d.%d", a,b,c,d);
	return 0;
}


/*
 * store human readable of IPv6 z in output
 * output MUST be large enough
 * compress = 0 ==> no adress compression
 * compress = 1 ==> remove leading zeros
 * compress = 2 ==> FULL compression
 */
int addrv62str(ipv6 z, char *out_buffer, int compress) {
	int a, i, j;
	int skip, max_skip = 0;
	int skip_index, max_skip_index = 0;

	if (compress == 0) {
		a = sprintf(out_buffer, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x", z.n16[0], z.n16[1], z.n16[2], z.n16[3], z.n16[4], z.n16[5], z.n16[6], z.n16[7]);
		return a;
	} else if (compress == 1) {
		a = sprintf(out_buffer, "%x:%x:%x:%x:%x:%x:%x:%x", z.n16[0], z.n16[1], z.n16[2], z.n16[3], z.n16[4], z.n16[5], z.n16[6], z.n16[7]);
		return a;
	}
	/**
	 ** longest 0000 block sequence will be replaced
	 */
	skip = 0;
	for (i = 0; i < 8; i++) {
		if (z.n16[i] == 0) {
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
	if (skip >= max_skip) {
		/* happens in case address ENDS with :0000, we then left the loop without setting max_skip__ */
		max_skip =  skip;
		max_skip_index = skip_index;
	}
	debug(PARSEIPV6, 5, "can skip %d blocks at index %d\n", max_skip, max_skip_index);
	j = 0;
	for (i = 0; i < max_skip_index; i++) {
		a = sprintf(out_buffer + j, "%x:", z.n16[i]);
		j += a;
		debug(PARSEIPV6, 9, "building output  %d : %s\n", i, out_buffer);
	}
	if (max_skip > 0) {
		if (max_skip_index)
			a = sprintf(out_buffer + j, ":");
		else
			a = sprintf(out_buffer + j, "::");
		debug(PARSEIPV6, 9, "building output  %d : %s\n", i, out_buffer);
		j += a;
	}
	for (i = max_skip_index+max_skip; i < 7; i++) {
		a = sprintf(out_buffer + j, "%x:", z.n16[i]);
		debug(PARSEIPV6, 9, "building output  %d : %s\n", i, out_buffer);
		j += a;
	}
	if (i < 8) {
		a = sprintf(out_buffer + j, "%x", z.n16[i]);
		j += a;
	}
	out_buffer[j] = '\0';
	return j;
}

/* outbuffer must be large enough **/
int subnet2str(const struct subnet *s, char *out_buffer, int comp_level) {
	if (s->ip_ver == IPV4_A || s->ip_ver == IPV4_N)
		return addrv42str(s->ip, out_buffer);
	if (s->ip_ver == IPV6_A || s->ip_ver == IPV6_N)
		return addrv62str(s->ip6, out_buffer, comp_level);
	out_buffer[0] = '\0';
	return -1;
}

/* IPv4 only, mask to Dot Decimal Notation */
int mask2ddn(u32 mask, char *out) {
	int i;
	int s[4];

        for (i = 0; i < mask / 8; i++)
                s[i] = 255;
        s[i++] = 256 - (1 << (8 - (mask % 8)));
        for ( ; i < 4; i++)
                s[i] = 0;
        sprintf(out, "%d.%d.%d.%d", s[0], s[1], s[2], s[3]);
	return 0;
}
/*
 * input must be  IPv6
 * out must be at least 41 bytes
 */
int ipv6expand(const char *input, char *out) {
	int i,j;
	int do_skip = 0;
	int out_i = 0;
	char *s, *s2;
	char buffer[51];
	int stop = 0, count = 0, count2 = 0;

	strxcpy(buffer, input, sizeof(buffer));
	s = buffer;
	if ((s[0] == s[1]) && (s[0] == ':')) { /** loopback addr **/
		do_skip = 1;
		if (s[2] == '\0') { /* special :: */
			strcpy(out, "0000:0000:0000:0000:0000:0000:0000:0000");
			return strlen(out);
		}
		s2 = s+2;
		count2++;
	} else {
		s2 = s;
	}
	/** on essaie de compter le nombre de : (7max) et de :: (1max) */
	for (i = 1; i < strlen(s); i++) {
		if (s[i] == ':') {
			count++;
			if (s[i + 1] == ':') {
				count2++;
			}
		}
	}
	if (count > 7) {
		debug(PARSEIPV6, 1, "Bad ipv6 address %s,  too many ':', [%d]\n", s, count );
		return BAD_IP;
	}
	if (count2 > 1) {
		debug(PARSEIPV6, 1, "Bad ipv6 address %s,  too many '::', [%d]\n", s, count2 );
		return BAD_IP;
	}
	if (count2 == 0 && count != 7) {
		debug(PARSEIPV6, 1, "Bad ipv6 address %s,  not enough ':', [%d]\n", s, count );
		return BAD_IP;
	}

	debug(PARSEIPV6, 8, "counted %d ':' and %d '::'\n", count, count2);
	debug(PARSEIPV6, 8, "still to parse %s\n", s2);
	i = (do_skip ? 2 : 0); /* in case we start with :: */
	for (;i < 72; i++) {
		if (do_skip) {
			/* we refill the skipped 0000: nibbles */
			debug(PARSEIPV6, 9, "refill %d skipped '0000:' block \n", 8-count);
			for (j = 0; j < 8 - count; j++) {
				strcpy(out + out_i,"0000:");
				out_i += 5;
			}
			do_skip = 0;
		} else if (s[i] ==':'||s[i] == '\0') {
			if (s[i] == '\0')
				stop = 1;
			s[i] = '\0';
			debug(PARSEIPV6, 8, "padding block %s\n", s2);
			for (j = 0; j < 4-strlen(s2); j++) {
				debug(PARSEIPV6, 9, "padding with 0 out_offset %d\n", out_i);
				out[out_i]='0';
				out_i++;
			}
			for (j = 0; j < strlen(s2); j++) {
				debug(PARSEIPV6, 9, "copying '%c' out_offset %d\n", s2[j], out_i);
				out[out_i] = s2[j];
				out_i++;
			}
			if (stop) { /* we are here because s[i] was 0 before we replaced it*/
				out[out_i] = '\0';
				break;
			}

			out[out_i++]=':';
			i++;
			s2 = s+i;
			if (s2[0] == ':') { /* we found a ':: (compressed address) */
				do_skip = 1;
				s2++;
			}
			debug(PARSEIPV6, 9, "still to parse %s\n", s2);
		} else if (is_valid_ip_char(s[i]))
			continue;
		else {
			debug(PARSEIPV6, 2, "invalid char [%c] found in  block [%s] index %d\n", s[i], s2, i);
			return BAD_IP;
		}
	}
	return IPV6_A;
}

u32 string2mask(const char *string) {
	int i;
	u32 a;
	int count_dot=0;
	int truc[4];
	char s3[32];
	char *s;

	strxcpy(s3, string, sizeof(s3)); /** we copy because we dont want to modify the input string */
	s = s3;
	while (*s == ' ' || *s == '\t') /* enlevons les espaces*/
		s++;
	if (*s == '/') /* si il reste un / on le vire, mais on autorise */
		s++;
	if (*s == '\0')
		return BAD_MASK;
	for (i = 0; i < strlen(s); i++) {
		if (s[i] == '.' && s[i + 1] != '.') /* skipping consecutive . */
			count_dot++;
		else if (s[i] == ' ' || s[i] == '\t') {
			s[i] = '\0';
			break; /** fin de la chaine */
		} else if (isdigit(s[i]))
			continue;
		else {
			debug(PARSEIP, 5, "invalid mask %s,  contains [%c]\n", s, s[i]);
			return BAD_MASK;
		}
	}
	if (count_dot == 0) { /* notation en prefix */
		a = atoi(s);
		if (a > 128) {
			debug(PARSEIP, 5, "invalid mask %s,  too long\n", s);
			return BAD_MASK;
		}
		return a;
	}
	if (count_dot != 3) {
		debug(PARSEIP, 5, "invalid mask %s,  bad format\n", s);
		return BAD_MASK;
	}
	a = 0;
	sscanf(s,"%d.%d.%d.%d", truc, truc + 1, truc + 2, truc + 3);
	for (i=0; i < 4 ; i++) {
		if (i && ( (truc[i] > truc[i - 1])  || (truc[i] && truc[i] < 255 && truc[i - 1] < 255))   ) {
			debug(PARSEIP, 5, "invalid X.X.X.X mask %s\n",s);
			return BAD_MASK;
		}

		if (!isPower2(256 - truc[i])) {
			debug(PARSEIP, 5, "invalid X.X.X.X mask, contains '%d'\n", truc[i]);
			return BAD_MASK;
		}
		a += 8 - mylog2(256 - truc[i]);
	}
	return a;

}

static int get_single_ipv4(char *s, struct subnet *subnet) {
	int i, a;
	int count_dot = 0;
	int truc[4];

	while (*s == ' '||*s == '\t') /* remove  spaces*/
		s++;
	if (*s == '\0'||*s == '.') {
		debug(PARSEIP, 5, "invalid IP %s,  bad format\n", s);
		return BAD_IP;
	}
	for (i = 0; i < strlen(s); i++) {
		if (s[i] == '.')
			count_dot++;
		else if (s[i] == ' '||s[i] == '\t') {
			s[i] = '\0';
			break; /** fin de la chaine */
		} else if (isdigit(s[i]))
			continue;
		else {
			debug(PARSEIP, 5, "invalid IP %s,  contains [%c]\n", s, s[i]);
			return BAD_IP;
		}
	}
	if (count_dot != 3) {
		debug(PARSEIP, 5, "invalid IP %s,  bad format\n", s);
		return BAD_IP;
	}
	a = sscanf(s,"%d.%d.%d.%d", truc, truc + 1, truc + 2, truc + 3);
	if (a != 4) {
		debug(PARSEIP, 5, "invalid IP %s,  bad format\n", s);
		return BAD_IP;
	}
	for (i = 0; i < 4; i++) {
		if (truc[i] > 255) {
			debug(PARSEIP, 5, "invalid IP %s,  %d too big\n", s, truc[i]);
			return BAD_IP;
		}
	}
	subnet->ip = truc[0] * 256 * 256 * 256  + truc[1] * 256 * 256 + truc[2] * 256 + truc[3];
	subnet->mask = 32;
	return IPV4_A;
}

static int get_single_ipv6(char *s, struct subnet *subnet) {
	int i,j;
	int do_skip = 0;
	int out_i = 0;
	char *s2;
	int stop = 0;
	int count = 0, count2 = 0;

	subnet->ip_ver = IPV6_A;
	if ((s[0] == s[1]) && (s[0] == ':')) { /** loopback addr **/
		do_skip = 1;
		if (s[2] == '\0') { /* special :: */
			memset(&subnet->ip6, 0, sizeof(subnet->ip6));
			subnet->mask = 128;
			return IPV6_A;
		}
		s2 = s+2;
		count2++;
	} else {
		s2 = s;
	}
	/** on essaie de compter le nombre de : (7max) et de :: (1max) */
	for (i = 1; i < strlen(s); i++) {
		if (s[i] == ':') {
			count++;
			if (s[i + 1] == ':') {
				count2++;
			}
		}
	}
	if (count > 7) {
		debug(PARSEIPV6, 1, "Bad ipv6 address %s, too many ':', [%d]\n", s, count );
		return BAD_IP;
	}
	if (count2 > 1) {
		debug(PARSEIPV6, 1, "Bad ipv6 address %s, too many '::', [%d]\n", s, count2 );
		return BAD_IP;
	}
	if (count2 == 0 && count != 7) {
		debug(PARSEIPV6, 1, "Bad ipv6 address %s, not enough ':', [%d]\n", s, count );
		return BAD_IP;
	}

	debug(PARSEIPV6, 8, "counted %d ':' and %d '::'\n", count, count2);
	debug(PARSEIPV6, 8, "still to parse %s\n", s2);
	i = (do_skip ? 2 : 0); /* in case we start with :: */
	for (;i < 72; i++) {
		if (do_skip) {
			/* we refill the skipped 0000: blocks */
			debug(PARSEIPV6, 9, "copying %d skipped '0000:' blocks\n", 8 - count);
			for (j = 0; j < 8 - count; j++) {
				subnet->ip6.n16[out_i] = 0;
				out_i++;
			}
			do_skip = 0;
		} else if (s[i] ==':'||s[i] == '\0') {
			if (s[i] == '\0')
				stop = 1;
			s[i] = '\0';
			debug(PARSEIPV6, 8, "copying block '%s' to block %d\n", s2, out_i);
			if (s2[0] == '\0') /* in case input ends with '::' */
				subnet->ip6.n16[out_i] = 0;
			else
				sscanf(s2, "%hx", &subnet->ip6.n16[out_i]);
			if (stop) /* we are here because s[i] was 0 before we replaced it*/
				break;

			out_i++;
			i++;
			s2 = s + i;
			if (s2[0] == ':') { /* we found a ':: (compressed address) */
				do_skip = 1;
				s2++;
			}
			debug(PARSEIPV6, 9, "still to parse %s\n", s2);
		} else if (is_valid_ip_char(s[i]))
			continue;
		else {
			debug(PARSEIPV6, 2, "invalid char [%c] found in  block [%s] index %d\n", s[i], s2, i);
			return BAD_IP;
		}
	}
	subnet->mask = 128;
	return IPV6_A;
}

int get_single_ip(const char *string, struct subnet *subnet) {
	int i;
	int may_ipv4 = 0, may_ipv6 = 0;
	char s3[52];
	char *s;

	strxcpy(s3, string, sizeof(s3)); /** we copy because we dont want to modify s2 */
	s = s3;
	while (*s == ' '||*s == '\t') /* remove spaces*/
		s++;
	if (*s == '\0')
		return BAD_IP;
	for (i = 0; i < strlen(s); i++) {
		if (s[i] == '.' && !may_ipv6) {
			may_ipv4 = 1;
			break;
		} else if (s[i] == ':') {
			may_ipv6 = 1;
			break;
		} else if (s[i] == ' ' || s[i] == '\t') {
			s[i] = '\0';
			break; /** fin de la chaine */
		} else if (isdigit(s[i]))
			continue;
		else if ((s[i] >= 'a' && s[i] <= 'f') || (s[i] >= 'A' && s[i] <= 'F')) {
			may_ipv6 = 2;
			continue; /* this is valid for IPv6 only */
		} else {
			debug(PARSEIP, 5, "invalid IP %s,  contains [%c]\n", s, s[i]);
			return BAD_IP;
		}
	}
	if (may_ipv4) {
		subnet->ip_ver = IPV4_A;
		return get_single_ipv4(s, subnet);
	}
	if (may_ipv6 == 1) {
		subnet->ip_ver = IPV6_A;
		return get_single_ipv6(s, subnet);
	}

	debug(PARSEIP, 5, "invalid IPv4 or IPv6 : %s\n", s);
	return BAD_IP;
}

/*
 * returns :
 *    IPV4_A : une IPv4 sans masque
 *    IPV4_N : une IPv4 + masque
 *    IPV6_A : une IPv6 sans masque
 *    IPV6_N : une IPv6 + masque
 *    BAD_IP, BAD_MASK sinon
 */
int get_subnet_or_ip(const char *string, struct subnet *subnet) {
	int i, a;
	u32 mask;
	int count_slash = 0;
	char *s, *save_s;
	char s2[128];

	strxcpy(s2, string, sizeof(s2));
	s = s2;

	while (*s == ' '||*s== '\t') /* enlevons les espaces*/
		s++;
	if (*s == '\0'||*s == '/') {
		debug(PARSEIP, 5, "invalid prefix %s, null IP\n", s);
		return BAD_IP;
	}
	debug(PARSEIP, 9, "prefix %s length %d\n", s, (int)strlen(s));
	for (i = 0; i < strlen(s); i++) {
		if (s[i] == '/')
			count_slash++;
		else if (is_valid_ip_char(s[i])||s[i] == '.'||s[i] == ':'||s[i] == ' ')
			continue;
		else {
			debug(PARSEIP, 5, "invalid prefix %s,  contains [%c]\n", s, s[i]);
			return BAD_IP;
		}
	}

	if (count_slash == 0) {
		debug(PARSEIP, 5, "trying to parse ip %s\n", s);
		return get_single_ip(s, subnet);
	} else if (count_slash == 1) {
		debug(PARSEIP, 5, "trying to parse ip/mask %s\n", s);
		s = strtok_r(s, "/", &save_s);
		a = get_single_ip(s, subnet);
		if (a == BAD_IP) {
			debug(PARSEIP, 5, "bad IP %s\n", s);
			return a;
		}
		s = strtok_r(NULL, "/", &save_s);
		mask = string2mask(s);
		if (mask == BAD_MASK) {
			debug(PARSEIP, 5, "bad mask %s\n", s);
			return BAD_MASK;
		}
		subnet->mask = mask;
		if (a == IPV4_A)
			return IPV4_N;
		else if (a == IPV6_A)
			return IPV6_N;
	}
	debug(PARSEIP, 5, "bad prefix format : %s\n", s);
	return BAD_IP;
}


int subnet_is_superior(struct subnet *s1, struct subnet *s2) {
	char buffer1[51], buffer2[51];
	int i, res;

	subnet2str(s1, buffer1, 2);
	subnet2str(s2, buffer2, 2);
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
		debug(ADDRCOMP, 5, "%s %c %s\n", buffer1, (res ? '>' : '<' ), buffer2);
		return res;
	}

	if (s1->ip_ver == IPV6_A) {
		if (is_equal_ipv6(s1->ip6, s2->ip6)) {
			if (s1->mask < s2->mask)
				res = 1;
			else
				res = 0;
		} else {
			for (i = 0; i < 4; i++) {
				if (s1->ip6.n32[i] < s2->ip6.n32[i]) {
					res = 1;
					break;
				} else if (s1->ip6.n32[i] > s2->ip6.n32[i]) {
					res = 0;
					break;
				}
			}
		}
		debug(ADDRCOMP, 5, "%s %c %s\n", buffer1, (res ? '>' : '<' ), buffer2);
		return res;
	}
	debug(ADDRCOMP, 1, "Invalid comparison for %s ipver %d\n", buffer1, s1->ip_ver);
	return -1;
}

/* try to aggregate s1 & s2, putting the result in s3 if possible
 * returns negative if impossible to aggregate, positive if possible */
static int aggregate_subnet_ipv4(const struct subnet *s1, const struct subnet *s2, struct subnet *s3) {
	ipv4 a, b;
	char buffer1[51], buffer2[51], buffer3[51];

	subnet2str(s1, buffer1, 2);
	subnet2str(s2, buffer2, 2);
	if (s1->mask != s2->mask) {
		debug(AGGREGATE, 5, "different masks for %s/%u and %s/%u, can't aggregate\n", buffer1, s1->mask, buffer2, s2->mask);
		return -1;
	}
	a = s1->ip >> (32 - s1->mask);
	b = s2->ip >> (32 - s1->mask);
	if (a == b) {
		debug(AGGREGATE, 6, "same subnet %s/%u\n", buffer1, s1->mask);
		s3->ip_ver = IPV4_A;
		s3->ip     = s1->ip;
		s3->mask   = s1->mask;
		return 1;
	}
	if (a >> 1 != b >> 1) {
		debug(AGGREGATE, 5, "cannot aggregate %s/%u and %s/%u\n", buffer1, s1->mask, buffer2, s2->mask);
		return -1;
	}
	s3->ip_ver = IPV4_A;
	s3->mask = s1->mask - 1;
	s3->ip   = (a >> 1) << (32 - s3->mask);
	subnet2str(s3, buffer3, 2);
	debug(AGGREGATE, 5, "can aggregate %s/%u and %s/%u into : %s/%u\n", buffer1, s1->mask, buffer2, s2->mask, buffer3, s3->mask);
	return 1;
}

static int aggregate_subnet_ipv6(const struct subnet *s1, const struct subnet *s2, struct subnet *s3) {
	ipv6 a, b;
	char buffer1[51], buffer2[51], buffer3[51];

	subnet2str(s1, buffer1, 2);
	subnet2str(s2, buffer2, 2);
	if (s1->mask != s2->mask) {
		debug(AGGREGATE, 5, "different masks for %s/%u and %s/%u, can't aggregate\n", buffer1, s1->mask, buffer2, s2->mask);
		return -1;
	}
	memcpy(&a, &s1->ip6, sizeof(a));
	memcpy(&b, &s2->ip6, sizeof(a));
	shift_right(a.n16, 8, (128 - s1->mask));
	shift_right(b.n16, 8, (128 - s1->mask));
	if (is_equal_ipv6(a, b)) {
		debug(AGGREGATE, 5, "same subnet %s/%u\n", buffer1, s1->mask);
		memcpy(s3, s1, sizeof(struct subnet));
		return 1;
	}
	shift_right(a.n16, 8, 1);
	shift_right(b.n16, 8, 1);
	if (!is_equal_ipv6(a, b)) {
		debug(AGGREGATE, 5, "cannot aggregate %s/%u and %s/%u\n", buffer1, s1->mask, buffer2, s2->mask);
		return -1;
	}
	s3->mask = s1->mask - 1;
	shift_left(a.n16, 8, 128 - s3->mask);
	s3->ip_ver = IPV6_A;
	memcpy(&s3->ip6, &a, sizeof(a));
	subnet2str(s3, buffer3, 2);
	debug(AGGREGATE, 5, "can aggregate %s/%u and %s/%u into : %s/%u\n", buffer1, s1->mask, buffer2, s2->mask, buffer3, s3->mask);
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

