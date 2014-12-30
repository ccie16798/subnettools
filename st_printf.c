/*
 * IPv4, IPv6 subnet/routes printing functions
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

#define ST_VSPRINTF_BUFFER_SIZE 2048

#define SET_IP_COMPRESSION_LEVEL(__c) do { \
	if (__c >= '0' && __c <= '3') { \
		compression_level = __c - '0'; \
		i++; \
	} else \
		compression_level = 3; \
	} while (0);

void fprint_route(const struct route *r, FILE *output, int compress_level) {
	char buffer[52];
	char buffer2[52];
	struct subnet s;

	subnet2str(&r->subnet, buffer, sizeof(buffer), compress_level);
	copy_ipaddr(&s.ip_addr, &r->gw);
	s.ip_ver = r->subnet.ip_ver;
	subnet2str(&s, buffer2, sizeof(buffer2), 2);
	fprintf(output, "%s;%d;%s;%s;%s\n", buffer, r->subnet.mask, r->device, buffer2, r->comment);
}
/*
 * a very specialized function to print a struct route */
void fprint_route_fmt(const struct route *r, FILE *output, const char *fmt) {
	int i, j, a ,i2, compression_level;
	char c;
	char outbuf[512 + 140];
	char buffer[128], buffer2[128], temp[32];
	struct subnet sub;
	char BUFFER_FMT[32];
	int field_width;
	struct subnet v_sub;
	/* %I for IP */
	/* %m for mask */
	/* %D for device */
	/* %g for gateway */
	/* %C for comment */
	i = 0;
	j = 0; /* index in outbuf */
	while (1) {
		c = fmt[i];
		debug(FMT, 5, "Still to parse : '%s'\n", fmt + i);
		if (j > sizeof(outbuf) - 140) { /* 128 is the max size (comment) */
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
			debug(FMT, 9, "Field width String is '%s'\n", temp);
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
				case 'U': /* upper subnet */
				case 'L': /* lower subnet */
				case 'I': /* IP address */
				case 'B': /* last IP Address of the subnet */
				case 'N': /* network adress of the subnet */
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					copy_subnet(&v_sub, &r->subnet);
					if (fmt[i2] == 'B')
						last_ip(&v_sub);
					else if (fmt[i2] == 'N')
						first_ip(&v_sub);
					else if (fmt[i2] == 'L')
						previous_subnet(&v_sub);
					else if (fmt[i2] == 'U')
						next_subnet(&v_sub);
					subnet2str(&v_sub, buffer, sizeof(buffer), compression_level);
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'P': /* Prefix */
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					copy_subnet(&v_sub, &r->subnet);
					subnet2str(&v_sub, buffer2, sizeof(buffer2), compression_level);
					sprintf(buffer, "%s/%d", buffer2, (int)v_sub.mask);
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'G':
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					copy_ipaddr(&sub.ip_addr, &r->gw);
					sub.ip_ver = r->subnet.ip_ver;
					subnet2str(&sub, buffer, sizeof(buffer2), compression_level);
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

/*
 * sadly a lot of code if common with fprint_route_fmt
 * But this cannot really be avoided
 */
static int st_vsprintf(char *out, const char *fmt, va_list ap)  {
	int i, j, a ,i2, compression_level;
	char c;
	char outbuf[ST_VSPRINTF_BUFFER_SIZE];
	char buffer[128], temp[32];
	char BUFFER_FMT[32];
	int field_width;
	/* variables from va_list */
	struct  subnet  v_sub;
	struct  ip_addr v_addr;
	char *v_s;
	int v_int;
	char v_c;
	unsigned v_unsigned;
	long v_long;
	unsigned long v_ulong;
	/* %a for ip_addr */
	/* %I for subnet_IP */
	/* %m for subnet_mask */
	i = 0;
	j = 0; /* index in outbuf */
	while (1) {
		c = fmt[i];
		debug(FMT, 5, "Still to parse : '%s'\n", fmt + i);
		if (j > sizeof(outbuf) - 140) { /* 128 is the max size */
			debug(FMT, 1, "output buffer maybe too small, truncating\n");
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
			debug(FMT, 9, "Field width String is '%s'\n", temp);
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
					v_sub = va_arg(ap, struct subnet);
					if (v_sub.ip_ver == IPV4_A)
						mask2ddn(v_sub.mask, buffer);
					else
						sprintf(buffer, "%d", (int)v_sub.mask);
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'm':
					v_sub = va_arg(ap, struct subnet);
					sprintf(buffer, "%d", (int)v_sub.mask);
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 's': /* almost standard %s printf, except that size is limited to 128 */
					v_s = va_arg(ap, char *);
					strxcpy(buffer, v_s, sizeof(buffer) - 1);
					if (strlen(v_s) >= sizeof(buffer) - 1)
						debug(FMT, 1, "truncating string '%s' to %d bytes\n", v_s, (int)(sizeof(buffer) - 1));
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'd':
					v_int = va_arg(ap, int);
					sprintf(buffer, "%d", v_int);
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'c':
					v_c = va_arg(ap, int);
					outbuf[j] = v_c;
					a++;
					break;
				case 'u':
					v_unsigned = va_arg(ap, unsigned);
					sprintf(buffer, "%u", v_unsigned);
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'l':
					if (fmt[i2 + 1] == 'u') {
						v_ulong = va_arg(ap, unsigned long);
						sprintf(buffer, "%lu", v_ulong);
						i++;
					} else {
						v_long = va_arg(ap, long);
						sprintf(buffer, "%ld", v_long);
					}
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'a':
					v_addr = va_arg(ap, struct ip_addr);
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					/* safegard a little */
					if (v_addr.ip_ver == IPV4_A || v_addr.ip_ver == IPV6_A)
						addr2str(&v_addr, buffer, sizeof(buffer), compression_level);
					else
						strcpy(buffer,"<Invalid IP>");
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'U': /* upper subnet */
				case 'L': /* lower subnet */
				case 'I': /* IP address */
				case 'B': /* last IP Address of the subnet */
				case 'N': /* network adress of the subnet */
					v_sub = va_arg(ap, struct subnet);
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);

					if (v_sub.ip_ver == IPV4_A || v_sub.ip_ver == IPV6_A) {
						if (fmt[i2] == 'B')
							last_ip(&v_sub);
						else if (fmt[i2] == 'N')
							first_ip(&v_sub);
						else if (fmt[i2] == 'L')
							previous_subnet(&v_sub);
						else if (fmt[i2] == 'U')
							next_subnet(&v_sub);
						subnet2str(&v_sub, buffer, sizeof(buffer), compression_level);
					} else
						strcpy(buffer, "<Invalid IP>");
					a += sprintf(outbuf + j, BUFFER_FMT, buffer);
					break;
				case 'P': /* Prefix */
					v_sub = va_arg(ap, struct subnet);
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);

					if (v_sub.ip_ver == IPV4_A || v_sub.ip_ver == IPV6_A) {
						char buffer2[128];
						subnet2str(&v_sub, buffer2, sizeof(buffer2), compression_level);
						sprintf(buffer, "%s/%d", buffer2, (int)v_sub.mask);
					} else
						strcpy(buffer, "<Invalid IP/mask>");
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
	return sprintf(out, "%s", outbuf);
}

int st_sprintf(char *out, const char *fmt, ...) {
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = st_vsprintf(out, fmt, ap);
	va_end(ap);
	return ret;
}

void st_fprintf(FILE *f, const char *fmt, ...) {
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, fmt);
	st_vsprintf(buffer, fmt, ap);
	va_end(ap);
	fprintf(f, "%s", buffer);
}

void st_printf(const char *fmt, ...) {
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, fmt);
	st_vsprintf(buffer, fmt, ap);
	va_end(ap);
	printf("%s", buffer);
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
