/*
 *  IPv4, IPv6 subnet/routes printing functions 
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
#include "debug.h"
#include "iptools.h"
#include "bitmap.h"
#include "utils.h"

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
