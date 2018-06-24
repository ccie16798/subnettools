/*
 * IPv4, IPv6 subnet/routes printing functions
 *
 * Copyright (C) 2014-2018 Etienne Basset <etienne POINT basset AT ensta POINT org>
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
#include "st_routes.h"
#include "bitmap.h"
#include "utils.h"
#include "st_object.h"
#include "bgp_tool.h"
#include "ipam.h"
#include "st_printf.h"

#define ST_VSPRINTF_BUFFER_SIZE   2048
#define ST_PRINTF_MAX_STRING_SIZE 256


sprint_signed(short)
sprint_signed(int)
sprint_signed(long)
sprint_hex(short)
sprint_hex(int)
sprint_hex(long)
sprint_unsigned(short)
sprint_unsigned(int)
sprint_unsigned(long)

/* we define a few MACRO to lessen the code duplication between :
 *  - fprint_route_fmt
 *  - fprint_bgproute_fmt
 *  - st_vsprintf
 */
#define SET_IP_COMPRESSION_LEVEL(__c) do { \
	if (__c >= '0' && __c <= '4') { \
		compression_level = __c - '0'; \
		i++; \
	} else \
		compression_level = 3; \
	} while (0)


#define BLOCK_ESCAPE_CHAR \
	do { \
		switch (fmt[i + 1]) { \
		case '\0': \
			outbuf[j++] = '\\'; \
			debug(FMT, 2, "End of String after a %c\n", '\\'); \
			i--; \
			break; \
		case 'n': \
			outbuf[j++] = '\n'; \
			break; \
		case 't': \
			outbuf[j++] = '\t'; \
			break; \
		case ' ': \
			outbuf[j++] = ' '; \
			break; \
		default: \
			 debug(FMT, 2, "%c is not a valid char after a %c\n", fmt[i + 1], '\\'); \
			outbuf[j++] = fmt[i + 1]; \
			break; \
		} \
		i += 2; \
	} while (0)

	/* this block computes field field between '%' and conversion specifier */
#define BLOCK_FIELD_WIDTH \
	do { \
		i2 = i + 1; \
		pad_left = 0; \
		field_width = 0; \
		/* try to get a field width (if any) */ \
		if (fmt[i2] == '\0') { \
			debug(FMT, 2, "End of String after a '%c'\n", '%'); \
			outbuf[j++] = '%'; \
			break; \
		} else if (fmt[i2] == '-') { \
			pad_left = 1; \
			i2++; \
		} \
		if (fmt[i2] == '0') { \
			pad_value = '0'; \
			i2++; \
		} \
		while (isdigit(fmt[i2])) { \
			field_width *= 10; \
			field_width += fmt[i2] - '0'; \
			i2++; \
		} \
		debug(FMT, 6, "Field width Int is '%d', padding is %s\n", field_width, \
				(pad_left ? "left" : "right")); \
		i = i2 - 1; \
	} while (0)

/* if fprint_route_fmt or fprint_bgp_route_fmt are called with route == NULL
 * it will print a subnet_file or bgp_file HEADER
 */
#define PRINT_FILE_HEADER(__val) ({ \
	if (r == NULL) { \
		strcpy(buffer, #__val); \
		res = strlen(buffer); \
		res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer, \
				res, field_width, pad_left, pad_value); \
		j += res; \
		break; \
	} })

void fprint_route(FILE *output, const struct route *r, int compress_level)
{
	char buffer[130];
	char buffer2[130];
	int i;

	subnet2str(&r->subnet, buffer, sizeof(buffer), compress_level);
	addr2str(&r->gw, buffer2, sizeof(buffer2), 2);
	fprintf(output, "%s;%d;%s;%s;%s\n",
			buffer, r->subnet.mask, r->device, buffer2, r->ea[0].value);
	for (i = 1; i < r->ea_nr; i++)
		fprintf(output, "%s%c", r->ea[i].value, (i == r->ea_nr - 1 ? '\n' : ';'));
}

static inline void pad_n(char *s, int n, char c)
{
	int i;

	for (i = 0; i < n; i++)
		s[i] = c;
}

/*
 * print 'buffer' into 'out' padding it if necessary
 * Don't copy more than 'len - 1' chars into out to preserve one byte for NULL
 * pad_buffer_out don't add a NUL char
 * @out        : outpuf buffer
 * @len        : out length
 * @buffer     : buffer containing string to print
 * @buff_size  : strlen(buffer)
 * @field_witdh: the requested size of output string
 * @pad_left   : requires padding on left size
 * @c	       : the padding char
 * returns the number of copied chars
 */
static inline int pad_buffer_out(char *out, size_t len, const char *buffer, size_t buff_size,
		size_t field_width, int pad_left, char c)
{
	int res;

	debug(FMT, 7, "Padding : len=%d, buff_size=%d, field_width=%d\n", (int)len,
			(int)buff_size, (int)field_width);
	if (len == 0)
		return 0;
	/* if buffer size is larger than field width, no need to pad */
	if (buff_size > field_width) {
		res = min(len - 1, buff_size);
		memcpy(out, buffer, res);
	} else if (pad_left) {
		if (len - 1 <= buff_size) { /* no more room in 'out' */
			res = len - 1;
			memcpy(out, buffer, res);
		} else {
			strcpy(out, buffer);
			res = min(field_width, len - 1);
			pad_n(out + buff_size, res - buff_size, c);
		}
	} else { /* pad right */
		if (len - 1 <= field_width - buff_size) {
			pad_n(out, len, c);
			res = len - 1;
		} else {
			pad_n(out, field_width - buff_size, c);
			res = min(field_width, len - 1);
			memcpy(out + field_width - buff_size, buffer, res);
		}
	}
	return res;
}

/*
 * helper to print Extended Attributes
 * @outbuf     : the output buffer
 * @buffer_len : the output buffer length
 * @fmt        : format buffer
 * @i          : index of the format buffer
 * @field_width: min length printed
 * @pad_left   : how to pad
 * @ea, @ea_nr : pointer to Extended Attributes and number
 * @header     : do we want to print EA or EA names
 * returns :
 *	number of printed chars in outbuf
 */
static int __print_ea(char *outbuf, size_t buffer_len,
			const char *fmt, int *i,
			int field_width, int pad_left,
			struct ipam_ea *ea, int ea_nr,
			int header)
{
	int k, res;
	char sep;
	char buffer[ST_PRINTF_MAX_STRING_SIZE];
	int ea_num;
	int j = 0;

	/* print all extended attributes */
	if (fmt[*i + 2] == '#') {
		sep = fmt[*i + 3];
		if (sep == '\0') /* set the default separator */
			sep = ';';
		for (k = 0; k < ea_nr; k++) {
			if (header) {
				if (ea[k].name == NULL) { /* happens only on programming errors */
					buffer[0] = '\0';
					fprintf(stderr, "BUG, EA#%d name is NULL\n", k);
					res = 0;
				} else
					res = strxcpy(buffer, ea[k].name,
							sizeof(buffer));
			} else {
				if (ea[k].value == NULL) {
					buffer[0] = '\0';
					res = 0;
				} else
					res = strxcpy(buffer, ea[k].value,
							sizeof(buffer));
			}
			if (res >= sizeof(buffer)) {
				debug(FMT, 1, "Warning, '%s' is truncated\n",
						buffer);
				res = sizeof(buffer);
			}
			res = pad_buffer_out(outbuf + j, buffer_len - j,
					buffer, res, field_width, pad_left, ' ');
			j += res;
			if (j == buffer_len - 1) {
				debug(FMT, 1, "Stopping after %d Extended Attributes\n", k + 1);
				break;
			} else if (j >= buffer_len) {
				fprintf(stderr, "BUG in %s, buffer overflow %d > %d\n", __func__,
					j, (int)buffer_len);
				break;
			}
			if (k != ea_nr - 1) {
				outbuf[j] = sep;
				j++;
			}
		}
		*i += 1;
		return j;
	} else if (isdigit(fmt[*i + 2])) {
		/* print just One Extended Attribute */
		ea_num = fmt[*i + 2] - '0';
		*i += 1;
		while (isdigit(fmt[*i + 2])) {
			ea_num *= 10;
			ea_num += fmt[*i + 2] - '0';
			*i += 1;
		}
		if (ea_num >= ea_nr) {
			debug(FMT, 2, "Invalid Extended Attribute number #%d, max %d\n",
					ea_num, ea_nr);
			return 0;
		}
		if (header) {
			if (ea[ea_num].name == NULL) { /* happens only on programming errors */
				fprintf(stderr, "BUG, EA#%d name is NULL\n", ea_num);
				buffer[0] = '\0';
				res = 0;
			} else
				res = strxcpy(buffer, ea[ea_num].name, sizeof(buffer));
		} else {
			if (ea[ea_num].value == NULL) {
				buffer[0] = '\0';
				res = 0;
			} else
				res = strxcpy(buffer, ea[ea_num].value,
						sizeof(buffer));
		}
		if (res >= sizeof(buffer)) {
			debug(FMT, 1, "Warning, '%s' is truncated\n", buffer);
			res = sizeof(buffer);
		}
		res = pad_buffer_out(outbuf,  buffer_len, buffer,
				res, field_width, pad_left, ' ');
		return res;
	}
	debug(FMT, 1, "Invalid char '%c' after %%O\n", fmt[*i + 2]);
	return 0;
}

#define IPAM_HEADER(__val) ({ \
	if (header) { \
		strcpy(buffer, __val); \
		res = strlen(buffer); \
		res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer, \
				res, field_width, pad_left, pad_value); \
		j += res; \
		break; \
	} \
	})

/* a very specialized function to print a struct route */
static int __fprint_route_fmt(FILE *output, const struct route *r, const char *fmt, int header)
{
	int i, j, i2, compression_level;
	int res, pad_left;
	char c;
	char outbuf[1024];
	char buffer[ST_PRINTF_MAX_STRING_SIZE];
	char buffer2[ST_PRINTF_MAX_STRING_SIZE / 2];
	struct subnet sub;
	int field_width;
	struct subnet v_sub;
	char pad_value;
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
		if (j >= sizeof(outbuf) - 1) {
			fprintf(stderr, "BUG in %s, buffer overrun, j=%d len=%d\n",
					__func__, j,
					 (int)sizeof(outbuf));
			break;
		/* must reserve one byte for '\n', one byte for '\0' */
		} else if (j == sizeof(outbuf) - 2) {
			debug(FMT, 2, "Output buffer is full, stopping\n");
			break;
		}
		if (c == '\0')
			break;
		if (c == '%') {
			BLOCK_FIELD_WIDTH;
			/*in fprint_route, pad value is always a space */
			pad_value = ' ';
			switch (fmt[i2]) {
			case '\0':
				outbuf[j] = '%';
				debug(FMT, 2, "End of String after a %c\n", '%');
				j++;
				i--;
				break;
			case 'M':
				IPAM_HEADER("mask");
				if (r->subnet.ip_ver == IPV4_A)
					res = mask2ddn(r->subnet.mask, buffer, sizeof(buffer));
				else if (r->subnet.ip_ver == IPV6_A)
					res = sprint_uint(buffer, r->subnet.mask);
				else {
					strcpy(buffer, "<Invalid mask>");
					res = strlen(buffer);
				}
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1,
						buffer, res, field_width, pad_left, pad_value);
				j += res;
				break;
			case 'm':
				IPAM_HEADER("mask");
				if (r->subnet.ip_ver == IPV4_A || r->subnet.ip_ver == IPV6_A)
					res = sprint_uint(buffer, r->subnet.mask);
				else {
					strcpy(buffer, "<Invalid mask>");
					res = strlen(buffer);
				}
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1,
						buffer, res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'D':
				IPAM_HEADER("device");
				res = strlen(r->device);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1,
						r->device,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'C':
				IPAM_HEADER("comment");
				if (r->ea[0].value == NULL) {
					buffer[0] = '\0';
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1,
							buffer,
							0, field_width, pad_left, ' ');
				} else {
					res = strlen(r->ea[0].value);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1,
							r->ea[0].value,
							res, field_width, pad_left, ' ');
				}
				j += res;
				break;
			case 'U': /* upper subnet */
			case 'L': /* lower subnet */
			case 'I': /* IP address */
			case 'B': /* last IP Address of the subnet */
			case 'N': /* network adress of the subnet */
				IPAM_HEADER("prefix");
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
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'P': /* Prefix */
				IPAM_HEADER("prefix");
				SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
				subnet2str(&r->subnet, buffer2, sizeof(buffer2), compression_level);
				res = sprintf(buffer, "%s/%d", buffer2, (int)r->subnet.mask);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'G':
				IPAM_HEADER("GW");
				SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
				copy_ipaddr(&sub.ip_addr, &r->gw);
				sub.ip_ver = r->subnet.ip_ver;
				res = subnet2str(&sub, buffer, sizeof(buffer), compression_level);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'O': /* Extended Attribute */
				res = __print_ea(outbuf + j, sizeof(outbuf) - j - 1,
						fmt, &i,
						field_width, pad_left,
						r->ea, r->ea_nr,
						header);
				j += res;
				break;
			default:
				debug(FMT, 2, "%c is not a valid char after a %c\n", fmt[i2], '%');
				outbuf[j++] = '%';
				if (j < sizeof(outbuf) - 2)
					outbuf[j++] = fmt[i2];
				break;
			} /* switch */
			i += 2;
		} else if (c == '\\') {
			BLOCK_ESCAPE_CHAR;
		} else {
			outbuf[j++] = c;
			i++;
		}
	}
	outbuf[j++] = '\n';
	outbuf[j] = '\0';
	return fputs(outbuf, output);
}

int fprint_route_fmt(FILE *output, const struct route *r, const char *fmt)
{
	return __fprint_route_fmt(output, r, fmt, 0);
}

int fprint_route_header(FILE *output, const struct route *r, const char *fmt)
{
	return __fprint_route_fmt(output, r, fmt, 1);
}

static int __fprint_ipam_fmt(FILE *output, const struct ipam_line *r,
		const char *fmt, int header)
{
	int i, j, i2, compression_level;
	int res, pad_left;
	char c;
	char outbuf[1024];
	char buffer[ST_PRINTF_MAX_STRING_SIZE];
	char buffer2[ST_PRINTF_MAX_STRING_SIZE / 2];
	int field_width;
	struct subnet v_sub;
	char pad_value;
	/* %I for IP */
	/* %m for mask */
	/* %O0....9 for extanded attributes */
	i = 0;
	j = 0; /* index in outbuf */
	while (1) {
		c = fmt[i];
		debug(FMT, 5, "Still to parse : '%s'\n", fmt + i);
		if (j >= sizeof(outbuf) - 1) {
			fprintf(stderr, "BUG in %s, buffer overrun, j=%d len=%d\n",
					__func__, j,
					 (int)sizeof(outbuf));
			break;
		} else if (j == sizeof(outbuf) - 2) {
			debug(FMT, 2, "Output buffer is full, stopping\n");
			break;
		}
		if (c == '\0')
			break;
		if (c == '%') {
			BLOCK_FIELD_WIDTH;
			pad_value = ' '; /*in fprint_ipam, pad value is always a space */
			switch (fmt[i2]) {
			case '\0':
				outbuf[j] = '%';
				debug(FMT, 2, "End of String after a %c\n", '%');
				j++;
				i--;
				break;
			case 'M':
				IPAM_HEADER("mask");
				if (r->subnet.ip_ver == IPV4_A)
					res = mask2ddn(r->subnet.mask, buffer, sizeof(buffer));
				else if (r->subnet.ip_ver == IPV6_A)
					res = sprint_uint(buffer, r->subnet.mask);
				else {
					strcpy(buffer, "<Invalid mask>");
					res = strlen(buffer);
				}
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1,
						buffer,
						res, field_width, pad_left, pad_value);
				j += res;
				break;
			case 'm':
				IPAM_HEADER("mask");
				if (r->subnet.ip_ver == IPV4_A || r->subnet.ip_ver == IPV6_A)
					res = sprint_uint(buffer, r->subnet.mask);
				else {
					strcpy(buffer, "<Invalid mask>");
					res = strlen(buffer);
				}
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1,
						buffer,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'I': /* IP address */
				IPAM_HEADER("address");
				SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
				copy_subnet(&v_sub, &r->subnet);
				res = subnet2str(&v_sub, buffer, sizeof(buffer), compression_level);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1,
						buffer,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'P': /* Prefix */
				IPAM_HEADER("prefix");
				SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
				copy_subnet(&v_sub, &r->subnet);
				subnet2str(&v_sub, buffer2, sizeof(buffer2), compression_level);
				res = sprintf(buffer, "%s/%d", buffer2, (int)v_sub.mask);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1,
						buffer,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'O': /* Extended Attribute */
				res = __print_ea(outbuf + j, sizeof(outbuf) - j - 1,
						fmt, &i,
						field_width, pad_left,
						r->ea, r->ea_nr,
						header);
				j += res;
				break;
			default:
				debug(FMT, 2, "%c is not a valid char after a %%\n", fmt[i2]);
				outbuf[j++] = '%';
				if (j < sizeof(outbuf) - 2)
					outbuf[j++] = fmt[i2];
				break;
			} /* switch */
			i += 2;
		} else if (c == '\\') {
			BLOCK_ESCAPE_CHAR;
		} else {
			outbuf[j++] = c;
			i++;
		}
	}
	outbuf[j++] = '\n';
	outbuf[j] = '\0';
	return fputs(outbuf, output);
}

int fprint_ipam_fmt(FILE *output, const struct ipam_line *r, const char *fmt)
{
	return __fprint_ipam_fmt(output, r, fmt, 0);
}

int fprint_ipam_header(FILE *output, const struct ipam_line *r, const char *fmt)
{
	return __fprint_ipam_fmt(output, r, fmt, 1);
}

/* a very specialized function to print a struct bgp_route */
int fprint_bgproute_fmt(FILE *output, const struct bgp_route *r, const char *fmt)
{
	int i, j, i2, compression_level;
	int res, pad_left;
	char c;
	char outbuf[1024];
	char buffer[ST_PRINTF_MAX_STRING_SIZE];
	char buffer2[ST_PRINTF_MAX_STRING_SIZE / 2];
	struct subnet sub;
	int field_width;
	struct subnet v_sub;
	char pad_value;
	char *truc;
	/* %P for prefix
	 * %I for IP
	 * %m for mask
	 * %G for gateway
	 * %M for MED
	 * %L for LOCAL_PREF
	 * %A for AS_PATH
	 * %w for weight
	 * %o for origin
	 * %T for TYPE ('e' for eBGP, 'i' for iBGP)
	 * %B for "Best/No"
	 * %v for valid
	 */
	i = 0;
	j = 0; /* index in outbuf */
	while (1) {
		c = fmt[i];
		debug(FMT, 5, "Still to parse : '%s'\n", fmt + i);
		if (j >= sizeof(outbuf) - 1) {
			fprintf(stderr, "BUG in %s, buffer overrun, j=%d len=%d\n",
					__func__, j,
					 (int)sizeof(outbuf));
			break;
		} else if (j == sizeof(outbuf) - 2) {
			debug(FMT, 2, "Output buffer is full, stopping\n");
			break;
		}
		if (c == '\0')
			break;
		if (c == '%') {
			BLOCK_FIELD_WIDTH;
			/* in fprint_bgp_route, pad value is always a space */
			pad_value = ' ';
			switch (fmt[i2]) {
			case '\0':
				outbuf[j] = '%';
				debug(FMT, 2, "End of String after a %c\n", '%');
				j++;
				i--;
				break;
			case 'w':
				PRINT_FILE_HEADER(WEIGHT);
				res = sprint_uint(buffer, r->weight);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						res, field_width, pad_left, pad_value);
				j += res;
				break;
			case 'L':
				PRINT_FILE_HEADER(LOCAL_PREF);
				res = sprint_uint(buffer, r->LOCAL_PREF);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						res, field_width, pad_left, pad_value);
				j += res;
				break;
			case 'A':
				PRINT_FILE_HEADER(AS_PATH);
				res = strlen(r->AS_PATH);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1,
						r->AS_PATH,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'M':
				PRINT_FILE_HEADER(MED);
				res = sprint_uint(buffer, r->MED);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						res, field_width, pad_left, pad_value);
				j += res;
				break;
			case 'o':
				PRINT_FILE_HEADER(ORIGIN);
				buffer[0] = r->origin;
				buffer[1] = '\0';
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						1, field_width, pad_left, pad_value);
				j += res;
				break;
			case 'b':
				PRINT_FILE_HEADER(BEST);
				truc = (r->best ? "1" : "0");
				res = strlen(truc);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, truc,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'B':
				PRINT_FILE_HEADER(BEST);
				truc = (r->best ? "Best" : "No");
				res = strlen(truc);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, truc,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'v':
				PRINT_FILE_HEADER(V);
					outbuf[j] = (r->valid ? '1' : '0');
				j++;
				break;
			case 'T':
				PRINT_FILE_HEADER(Proto);
				truc = (r->type == 'i' ? "iBGP" : "eBGP");
				res = strlen(truc);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, truc,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'm':
				PRINT_FILE_HEADER(Mask);
				if (r->subnet.ip_ver == IPV4_A || r->subnet.ip_ver == IPV6_A)
					res = sprint_uint(buffer, r->subnet.mask);
				else {
					strcpy(buffer, "<Invalid mask>");
					res = strlen(buffer);
				}
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'I': /* IP address */
				PRINT_FILE_HEADER(IP);
				SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
				copy_subnet(&v_sub, &r->subnet);
				res = subnet2str(&v_sub, buffer, sizeof(buffer), compression_level);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'P': /* Prefix */
				PRINT_FILE_HEADER(prefix);
				SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
				copy_subnet(&v_sub, &r->subnet);
				subnet2str(&v_sub, buffer2, sizeof(buffer2), compression_level);
				res = sprintf(buffer, "%s/%d", buffer2, (int)v_sub.mask);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			case 'G':
				PRINT_FILE_HEADER(GW);
				SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
				copy_ipaddr(&sub.ip_addr, &r->gw);
				sub.ip_ver = r->subnet.ip_ver;
				res = subnet2str(&sub, buffer, sizeof(buffer), compression_level);
				res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j - 1, buffer,
						res, field_width, pad_left, ' ');
				j += res;
				break;
			default:
				debug(FMT, 2, "%c is not a valid char after a %c\n", fmt[i2], '%');
				outbuf[j++] = '%';
				if (j < sizeof(outbuf) - 2)
					outbuf[j++] = fmt[i2];
				break;
			} /* switch */
			i += 2;
		} else if (c == '\\') {
			BLOCK_ESCAPE_CHAR;
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
static int st_vsnprintf(char *outbuf, size_t len, const char *fmt, va_list ap,
		struct sto *o, int max_o)
{
	int i, j, i2, compression_level;
	int res;
	char c;
	char buffer[ST_PRINTF_MAX_STRING_SIZE];
	char buffer2[ST_PRINTF_MAX_STRING_SIZE / 2];
	int field_width;
	int pad_left, pad_value;
	int o_num;
	/* variables from va_list */
	struct  subnet  v_sub;
	struct  ip_addr v_addr;
	char *v_s;
	int v_int;
	char v_c;
	unsigned int v_unsigned;
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
			fprintf(stderr, "BUG in %s, buffer overrun, j=%d len=%d\n",
					__func__, j, (int)len);
			break;
		} else if (j == len - 1) {
			debug(FMT, 2, "Output buffer is full, stopping\n");
			break;
		}
		if (c == '\0')
			break;
		if (c == '%') {
			pad_value = ' ';
			BLOCK_FIELD_WIDTH;
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
				else if (v_sub.ip_ver == IPV6_A)
					res = sprint_uint(buffer, v_sub.mask);
				else {
					strcpy(buffer, "<Invalid mask>");
					res = strlen(buffer);
				}
				res = pad_buffer_out(outbuf + j, len - j, buffer, res,
						field_width, pad_left, ' ');
				j += res;
				break;
			case 'm':
				v_sub = va_arg(ap, struct subnet);
				/* safegard a little */
				if (v_sub.ip_ver == IPV4_A || v_sub.ip_ver == IPV6_A)
					res = sprint_uint(buffer, v_sub.mask);
				else {
					strcpy(buffer, "<Invalid mask>");
					res = strlen(buffer);
				}
				res = pad_buffer_out(outbuf + j, len - j, buffer, res,
						field_width, pad_left, ' ');
				j += res;
				break;
			case 's': /* almost standard %s printf, except that size is limited */
				v_s = va_arg(ap, char *);
				res = strxcpy(buffer, v_s, sizeof(buffer) - 1);

				if (res >= sizeof(buffer) - 1) {
					debug(FMT, 3, "truncating string '%s' to %d bytes\n",
							v_s, (int)(sizeof(buffer) - 1));
					res = sizeof(buffer) - 1;
				}
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
				v_unsigned = va_arg(ap, unsigned int);
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
			case 'h':/* half integers handling */
				if (fmt[i2 + 1] == 'u') {
					v_ushort = va_arg(ap, unsigned int);
					res = sprint_ushort(buffer, v_ushort);
				} else if (fmt[i2 + 1] == 'd') {
					v_short = va_arg(ap, int);
					res = sprint_short(buffer, v_short);
				} else if (fmt[i2 + 1] == 'x') {
					v_short = va_arg(ap, int);
					res = sprint_hexshort(buffer, v_short);
				} else {
					debug(FMT, 2, "Invalid format '%c' after '%%h'\n",
							fmt[i2 + 1]);
					break;
				}
				i++;
				res = pad_buffer_out(outbuf + j, len - j, buffer, res,
						field_width, pad_left, pad_value);
				j += res;
				break;
			case 'l': /* long stuff (like the bride would say) */
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
					debug(FMT, 2, "Invalid format '%c' after '%%l'\n",
							fmt[i2 + 1]);
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
					res = addr2str(&v_addr, buffer,
							sizeof(buffer), compression_level);
				else {
					strcpy(buffer, "<Invalid IP>");
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
					res = subnet2str(&v_sub, buffer,
							sizeof(buffer), compression_level);
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
					subnet2str(&v_sub, buffer2,
							sizeof(buffer2), compression_level);
					res = sprintf(buffer, "%s/%d", buffer2, (int)v_sub.mask);
				} else {
					strcpy(buffer, "<Invalid IP/mask>");
					res = strlen(buffer);
				}
				res = pad_buffer_out(outbuf + j, len - j, buffer, res,
						field_width, pad_left, ' ');
				j += res;
				break;
			case 'O': /* st_object */
				if (o == NULL)
					break;
				if (fmt[i + 2] == '#') { /* print all object */
					for (o_num = 0; o_num < max_o; o_num++) {
						res = sto2string(buffer, &o[o_num],
								sizeof(buffer), 3);
						if (res > 0)
							res = pad_buffer_out(outbuf + j,
									len - j, buffer, res,
									field_width, pad_left, ' ');
						else
							res = 0;
						j += res;
						outbuf[j++] = ';';
					} /* for */
					i++;
					break;
				}
				if (!isdigit(fmt[i + 2])) {
					debug(FMT, 3, "Invalid char '%c' after %%O\n",
							fmt[i + 2]);
					break;
				}
				o_num = 0;
				while (isdigit(fmt[i + 2])) {
					o_num *= 10;
					o_num += fmt[i + 2] - '0';
					i++;
				}
				if (o_num >= max_o) {
					debug(FMT, 3, "Invalid object number #%d, max %d\n",
							o_num, max_o);
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
				debug(FMT, 2, "%c is not a valid char after a %%\n", fmt[i2]);
				outbuf[j++] = '%';
				if (j < sizeof(outbuf) - 1)
					outbuf[j++] = fmt[i2];
				break;
			} /* switch */
			i += 2;
		} else if (c == '\\') {
			BLOCK_ESCAPE_CHAR;
		} else {
			outbuf[j++] = c;
			i++;
		}
	}
	outbuf[j] = '\0';
	return j;
}

int st_snprintf(char *out, size_t len, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = st_vsnprintf(out, len, fmt, ap, NULL, 0);
	va_end(ap);
	return ret;
}

int st_fprintf(FILE *f, const char *fmt, ...)
{
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, fmt);
	st_vsnprintf(buffer, sizeof(buffer), fmt, ap, NULL, 0);
	va_end(ap);
	return fputs(buffer, f);
}

int st_printf(const char *fmt, ...)
{
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, fmt);
	st_vsnprintf(buffer, sizeof(buffer), fmt, ap, NULL, 0);
	va_end(ap);
	return fputs(buffer, stdout);
}

int sto_snprintf(char *out, size_t len, const char *fmt, struct sto *o, int max_o, ...)
{
	va_list ap;
	int ret;

	va_start(ap, max_o);
	ret = st_vsnprintf(out, len, fmt, ap, o, max_o);
	va_end(ap);
	return ret;
}

int sto_fprintf(FILE *f, const char *fmt, struct sto *o, int max_o, ...)
{
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, max_o);
	st_vsnprintf(buffer, sizeof(buffer), fmt, ap, o, max_o);
	va_end(ap);
	return fputs(buffer, f);
}

int sto_printf(const char *fmt, struct sto *o, int max_o, ...)
{
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, max_o);
	st_vsnprintf(buffer, sizeof(buffer), fmt, ap, o, max_o);
	va_end(ap);
	return fputs(buffer, stdout);
}

void fprint_subnet_file(FILE *output, const struct subnet_file *sf, int compress_level)
{
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		fprint_route(output, &sf->routes[i], compress_level);
}

void fprint_subnet_file_fmt(FILE *output, const struct subnet_file *sf, const char *fmt)
{
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		fprint_route_fmt(output, &sf->routes[i], fmt);
}

void print_subnet_file(const struct subnet_file *sf, int compress_level)
{
	fprint_subnet_file(stdout, sf, compress_level);
}

void fprint_bgp_file_fmt(FILE *output, const struct bgp_file *sf, const char *fmt)
{
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		fprint_bgproute_fmt(output, &sf->routes[i], fmt);
}

void print_bgp_file_fmt(const struct bgp_file *sf, const char *fmt)
{
	fprint_bgp_file_fmt(stdout, sf, fmt);
}

void fprint_ipam_file_fmt(FILE *output, const struct ipam_file *sf, const char *fmt)
{
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		fprint_ipam_fmt(output, &sf->lines[i], fmt);
}

void print_ipam_file_fmt(const struct ipam_file *sf, const char *fmt)
{
	fprint_ipam_file_fmt(stdout, sf, fmt);
}
