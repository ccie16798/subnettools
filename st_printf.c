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
#include "st_object.h"

#define ST_VSPRINTF_BUFFER_SIZE 2048

#define SET_IP_COMPRESSION_LEVEL(__c) do { \
	if (__c >= '0' && __c <= '3') { \
		compression_level = __c - '0'; \
		i++; \
	} else \
		compression_level = 3; \
	} while (0);

sprint_signed(short)
sprint_signed(int)
sprint_signed(long)
sprint_hex(short)
sprint_hex(int)
sprint_hex(long)
sprint_unsigned(short)
sprint_unsigned(int)
sprint_unsigned(long)

void fprint_route(const struct route *r, FILE *output, int compress_level) {
	char buffer[52];
	char buffer2[52];

	subnet2str(&r->subnet, buffer, sizeof(buffer), compress_level);
	addr2str(&r->gw, buffer2, sizeof(buffer2), 2);
	fprintf(output, "%s;%d;%s;%s;%s\n", buffer, r->subnet.mask, r->device, buffer2, r->comment);
}

static void inline pad_n(char *s, int n, char c) {
	int i;
	for (i = 0; i < n; i++)
		s[i] = c;
}


/*
 * print 'buffer' into 'out'
 * pad the buffer if there's a field width, but don't copy more than 'len' chars into out
 * 'out' wont have a terminating Nul-byte
 */
static inline int pad_buffer_out(char *out, size_t len, const char *buffer, size_t buff_size,
		int field_width, int pad_left, char c) {
	int res;

	//buff_size = strlen(buffer);
	debug(FMT, 7, "Padding : len=%d, buff_size=%d, field_width=%d\n", (int)len, (int)buff_size, field_width);
	if (buff_size <= 0) {
		debug(FMT, 2, "Cannot pad an Invalid buffer\n");
		return 0;
	}
	if (buff_size > field_width) {
		res = min(len - 1, buff_size);
		memcpy(out, buffer, res);
	} else if (pad_left) {
		if (len - 1 <= buff_size) {
			res = len - 1;
			memcpy(out, buffer, res);
		} else {
			res = min(field_width, (int)len - 1);
			strcpy(out, buffer);
			pad_n(out + buff_size, res - buff_size, c);
		}
	} else {
		if (len - 1 <= field_width - buff_size) {
			pad_n(out, len, c);
			res = len - 1;
		} else {
			res = min(field_width, (int)len - 1);
			pad_n(out, field_width - buff_size, c);
			memcpy(out + field_width - buff_size, buffer, res);
		}
	}
	return res;
}
/*
 * a very specialized function to print a struct route */
int fprint_route_fmt(const struct route *r, FILE *output, const char *fmt) {
	int i, j, i2, compression_level;
	int res, pad_left;
	char c;
	char outbuf[512 + 140];
	char buffer[128], buffer2[128];
	struct subnet sub;
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
			pad_left = 0;
			field_width = 0;
			/* try to get a field width (if any) */
			if (fmt[i2] == '\0') {
				debug(FMT, 2, "End of String after a '%c'\n", '%');
				outbuf[j++] = '%';
				break;
			} else if (fmt[i2] == '-') {
				pad_left = 1;
				i2++;
			}
			if (fmt[i2] == '0') {
				/* pad_value = '0'; pad value not used */
				i2++;
			}
			while (isdigit(fmt[i2])) {
				field_width *= 10;
				field_width += fmt[i2] - '0';
				i2++;
			}
			debug(FMT, 6, "Field width Int is '%d', padding is %s\n", field_width, 
					(pad_left ? "left": "right") );
			i = i2 - 1;
			switch (fmt[i2]) {
				case '\0':
					outbuf[j] = '%';
					debug(FMT, 2, "End of String after a %c\n", '%');
					j++;
					i--;
					break;
				case 'M':
					if (r->subnet.ip_ver == IPV4_A)
						res = mask2ddn(r->subnet.mask, buffer, sizeof(buffer));
					else
						res = sprint_uint(buffer, r->subnet.mask);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'm':
					res = sprint_uint(buffer, r->subnet.mask);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'D':
					res = strlen(r->device);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, r->device,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'C':
					res = strlen(r->comment);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, r->comment,
							res, field_width, pad_left, ' ');
					j += res;
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
					res = subnet2str(&v_sub, buffer, sizeof(buffer), compression_level);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'P': /* Prefix */
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					copy_subnet(&v_sub, &r->subnet);
					subnet2str(&v_sub, buffer2, sizeof(buffer2), compression_level);
					res = sprintf(buffer, "%s/%d", buffer2, (int)v_sub.mask);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'G':
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					copy_ipaddr(&sub.ip_addr, &r->gw);
					sub.ip_ver = r->subnet.ip_ver;
					res = subnet2str(&sub, buffer, sizeof(buffer), compression_level);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				default:
					debug(FMT, 2, "%c is not a valid char after a %c\n", fmt[i2], '%');
					outbuf[j] = '%';
					outbuf[j + 1] = fmt[i2];
					j += 2;
			} //switch
			i += 2;
		} else if (c == '\\') {
			switch (fmt[i + 1]) {
				case '\0':
					outbuf[j++] = '\\';
					debug(FMT, 2, "End of String after a %c\n", '\\');
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
	outbuf[j++] = '\n';
	outbuf[j] = '\0';
	return fputs(outbuf, output);
}

/*
 * sadly a lot of code if common with fprint_route_fmt
 * But this cannot really be avoided
 */
static int st_vsnprintf(char *outbuf, size_t len, const char *fmt, va_list ap, struct sto *o, int max_o)  {
	int i, j, i2, compression_level;
	int res;
	char c;
	char buffer[128];
	int field_width;
	int pad_left, pad_value;
	int o_num;
	/* variables from va_list */
	struct  subnet  v_sub;
	struct  ip_addr v_addr;
	char *v_s;
	int v_int;
	char v_c;
	unsigned v_unsigned;
	long v_long;
	unsigned long v_ulong;
	short v_short;
	unsigned short v_ushort;

	i = 0; /* index in fmt */
	j = 0; /* index in outbuf */
	while (1) {
		c = fmt[i];
		debug(FMT, 5, "Still to parse in FMT : '%s'\n", fmt + i);
		if (j >= len) {
			fprintf(stderr, "BUG in %s, buffer overrun, j=%d len=%d\n", __FUNCTION__, j, (int)len);
			break;
		} else if (j == len - 1) {
			debug(FMT, 2, "Output buffer is full, stopping\n");
			break;
		}
		if (c == '\0')
			break;
		if (c == '%') {
			i2 = i + 1;
			pad_left = 0;
			pad_value = ' ';
			field_width = 0;
			/* try to get a field width (if any) */
			if (fmt[i2] == '\0') {
				debug(FMT, 2, "End of String after a '%c'\n", '%');
				outbuf[j++] = '%';
				break;
			} else if (fmt[i2] == '-') {
				pad_left = 1;
				i2++;
			}
			if (fmt[i2] == '0') {
				pad_value = '0';
				i2++;
			}
			while (isdigit(fmt[i2])) {
				field_width *= 10;
				field_width += fmt[i2] - '0';
				i2++;
			}
			debug(FMT, 6, "Field width Int is '%d', padding is %s\n", field_width, 
					(pad_left ? "left": "right") );
			i = i2 - 1;
			switch (fmt[i2]) {
				case '\0':
					outbuf[j] = '%';
					debug(FMT, 2, "End of String after a %c\n", '%');
					j += 1;
					i--;
					break;
				case 'M':
					v_sub = va_arg(ap, struct subnet);
					if (v_sub.ip_ver == IPV4_A)
						res = mask2ddn(v_sub.mask, buffer, sizeof(buffer));
					else
						res = sprint_uint(buffer, v_sub.mask);
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, ' ');
					j += res;
					break;
				case 'm':
					v_sub = va_arg(ap, struct subnet);
					res = sprint_uint(buffer, v_sub.mask);
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, ' ');
					j += res;
					break;
				case 's': /* almost standard %s printf, except that size is limited to 128 */
					v_s = va_arg(ap, char *);
					res = strxcpy(buffer, v_s, sizeof(buffer) - 1);

					if (strlen(v_s) >= sizeof(buffer) - 1)
						debug(FMT, 1, "truncating string '%s' to %d bytes\n", v_s, (int)(sizeof(buffer) - 1));
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, ' ');
					j += res;
					break;
				case 'd':
					v_int = va_arg(ap, int);
					res = sprint_int(buffer, v_int);
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, pad_value);
					j += res;
					break;
				case 'c':
					v_c = va_arg(ap, int);
					outbuf[j] = v_c;
					j++;
					break;
				case 'u':
					v_unsigned = va_arg(ap, unsigned);
					res = sprint_uint(buffer, v_unsigned);
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, pad_value);
					j += res;
					break;
				case 'x':
					v_unsigned = va_arg(ap, unsigned int);
					res = sprint_hexint(buffer, v_unsigned);
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, pad_value);
					j += res;
					break;
				case 'h':
					if (fmt[i2 + 1] == 'u') {
						v_ushort = va_arg(ap, unsigned);
						res = sprint_ushort(buffer, v_ushort);
					} else if (fmt[i2 + 1] == 'd') {
						v_short = va_arg(ap, int);
						res = sprint_short(buffer, v_short);
					} else if (fmt[i2 + 1] == 'x') {
						v_short = va_arg(ap, int);
						res = sprint_hexshort(buffer, v_short);
					} else {
						debug(FMT, 1, "Invalid format '%c' after '%%h'\n", fmt[i2 + 1]);
						break;
					}
					i++;
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, pad_value);
					j += res;
					break;
				case 'l':
					if (fmt[i2 + 1] == 'u') {
						v_ulong = va_arg(ap, unsigned long);
						res = sprint_ulong(buffer, v_ulong);
					} else if (fmt[i2 + 1] == 'd') {
						v_long = va_arg(ap, long);
						res = sprint_long(buffer, v_long);
					} else if (fmt[i2 + 1] == 'x') {
						v_ulong = va_arg(ap, unsigned long);
						res = sprint_hexlong(buffer, v_ulong);
					} else {
						debug(FMT, 1, "Invalid format '%c' after '%%l'\n", fmt[i2 + 1]);
						break;
					}
					i++;
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, pad_value);
					j += res;
					break;
				case 'a':
					v_addr = va_arg(ap, struct ip_addr);
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					/* safegard a little */
					if (v_addr.ip_ver == IPV4_A || v_addr.ip_ver == IPV6_A)
						res = addr2str(&v_addr, buffer, sizeof(buffer), compression_level);
					else {
						strcpy(buffer,"<Invalid IP>");
						res = strlen(buffer);
					}
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, pad_value);
					j += res;
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
						res = subnet2str(&v_sub, buffer, sizeof(buffer), compression_level);
					} else {
						strcpy(buffer, "<Invalid IP>");
						res = strlen(buffer);
					}
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, ' ');
					j += res;
					break;
				case 'P': /* Prefix */
					v_sub = va_arg(ap, struct subnet);
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);

					if (v_sub.ip_ver == IPV4_A || v_sub.ip_ver == IPV6_A) {
						char buffer2[128];
						subnet2str(&v_sub, buffer2, sizeof(buffer2), compression_level);
						res = sprintf(buffer, "%s/%d", buffer2, (int)v_sub.mask);
					} else {
						strcpy(buffer, "<Invalid IP/mask>");
						res = strlen(buffer);
					}
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, ' ');
					j += res;
					break;
				case 'O':
					if (o == NULL)
						break;
					o_num = 0;
					while (isdigit(fmt[i + 2])) {
						o_num *= 10;
						o_num += fmt[i + 2] - '0';
						i++;
					}
					if (o_num >= max_o) {
						debug(FMT, 1, "Invalid object number #%d, max %d\n", o_num, max_o);
						break;
					}
					res = sto2string(buffer, &o[o_num], sizeof(buffer), 3);
					/* check if we need to print an Invalid object */
					if (res > 0)
						res = pad_buffer_out(outbuf + j, len - j, buffer, res,
								field_width, pad_left, ' ');
					else
						res = 0;
					j += res;
					break;
				default:
					debug(FMT, 2, "%c is not a valid char after a %c\n", fmt[i2], '%');
					outbuf[j] = '%';
					outbuf[j + 1] = fmt[i2];
					j += 2;
			} //switch
			i += 2;
		} else if (c == '\\') {
			switch (fmt[i + 1]) {
				case '\0':
					outbuf[j++] = '\\';
					debug(FMT, 2, "End of String after a %c\n", '\\');
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
	return j;
}

int st_snprintf(char *out, size_t len, const char *fmt, ...) {
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = st_vsnprintf(out, len, fmt, ap, NULL, 0);
	va_end(ap);
	return ret;
}

int st_fprintf(FILE *f, const char *fmt, ...) {
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, fmt);
	st_vsnprintf(buffer, sizeof(buffer), fmt, ap, NULL, 0);
	va_end(ap);
	return fprintf(f, "%s", buffer);
}

int st_printf(const char *fmt, ...) {
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, fmt);
	st_vsnprintf(buffer, sizeof(buffer), fmt, ap, NULL, 0);
	va_end(ap);
	return printf("%s", buffer);
}

int sto_snprintf(char *out, size_t len, const char *fmt, struct sto *o, int max_o, ...) {
	va_list ap;
	int ret;

	va_start(ap, max_o);
	ret = st_vsnprintf(out, len, fmt, ap, o, max_o);
	va_end(ap);
	return ret;
}

int sto_fprintf(FILE *f, const char *fmt, struct sto *o, int max_o, ...) {
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, max_o);
	st_vsnprintf(buffer, sizeof(buffer), fmt, ap, o, max_o);
	va_end(ap);
	return fprintf(f, "%s", buffer);
}

void sto_printf(const char *fmt, struct sto *o, int max_o, ...) {
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, max_o);
	st_vsnprintf(buffer, sizeof(buffer), fmt, ap, o, max_o);
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
