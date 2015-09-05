/*
 * IPv4, IPv6 subnet/routes printing functions
 *
 * Copyright (C) 2014, 2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
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
#include "st_handle_csv_files.h"
#include "bgp_tool.h"
#include "ipam.h"

#define ST_VSPRINTF_BUFFER_SIZE 2048

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
	if (__c >= '0' && __c <= '3') { \
		compression_level = __c - '0'; \
		i++; \
	} else \
		compression_level = 3; \
	} while (0);


#define BLOCK_ESCAPE_CHAR \
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
	} \
	i += 2;

/* this block computes field field between '%' and conversion specifier */
#define BLOCK_FIELD_WIDTH \
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
			(pad_left ? "left": "right") ); \
	i = i2 - 1;

/* if fprint_route_fmt or fprint_bgp_route_fmt are called with route == NULL
 * it will print a subnet_file or bgp_file HEADER */
#define PRINT_FILE_HEADER(__val) \
	if (r == NULL) { \
		strcpy(buffer, #__val); \
		res = strlen(buffer); \
		res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer, \
				res, field_width, pad_left, pad_value); \
		j += res; \
		break; \
	}

void fprint_route(FILE *output, const struct route *r, int compress_level) {
	char buffer[52];
	char buffer2[52];
	int i;

	subnet2str(&r->subnet, buffer, sizeof(buffer), compress_level);
	addr2str(&r->gw, buffer2, sizeof(buffer2), 2);
	fprintf(output, "%s;%d;%s;%s;%s\n", buffer, r->subnet.mask, r->device, buffer2, r->ea[0].value);
	for (i = 1; i < r->ea_nr; i++)
		fprintf(output, "%s%c", r->ea[i].value, (i == r->ea_nr -1 ? '\n' : ';'));
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

	debug(FMT, 7, "Padding : len=%d, buff_size=%d, field_width=%d\n", (int)len, (int)buff_size, field_width);
	if (buff_size < 0) {
		debug(FMT, 1, "Cannot pad an Invalid buffer\n");
		return 0;
	}
	/* if buffer size is larger than field width, no need to pad */
	if (buff_size > field_width) {
		res = min(len - 1, buff_size);
		memcpy(out, buffer, res);
	} else if (pad_left) {
		if (len - 1 <= buff_size) { /* no more room in 'out' */
			res = len - 1;
			memcpy(out, buffer, res);
		} else {
			res = min(field_width, (int)len - 1); /* buf_len < len, but field_with can be > len */
			strcpy(out, buffer);
			pad_n(out + buff_size, res - buff_size, c);
		}
	} else { /* pad right */
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
#define IPAM_HEADER(__val)			\
	if (header) { \
		strcpy(buffer, __val); \
		res = strlen(buffer); \
		res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer, \
				res, field_width, pad_left, pad_value); \
		j += res; \
		break; \
	}
/* a very specialized function to print a struct ipam */
 /* a very specialized function to print a struct route */
static int __fprint_route_fmt(FILE *output, const struct route *r, const char *fmt, int header) {
	int i, j, i2, compression_level;
	int res, pad_left, ea_num;
	char c;
	char outbuf[512 + 140];
	char buffer[128], buffer2[128];
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
		if (j >= sizeof(outbuf)) {
			fprintf(stderr, "BUG in %s, buffer overrun, j=%d len=%d\n", __FUNCTION__, j,
					 (int)sizeof(outbuf));
			break;
		} else if (j == sizeof(outbuf) - 1) {
			debug(FMT, 2, "Output buffer is full, stopping\n");
			break;
		}
		if (c == '\0')
			break;
		if (c == '%') {
			BLOCK_FIELD_WIDTH
			pad_value = ' '; /*in fprint_route, pad value is alwys a space */
			switch (fmt[i2]) {
				case '\0':
					outbuf[j] = '%';
					debug(FMT, 2, "End of String after a %c\n", '%');
					j++;
					i--;
					break;
				case 'M':
					IPAM_HEADER("mask")
					if (r->subnet.ip_ver == IPV4_A)
						res = mask2ddn(r->subnet.mask, buffer, sizeof(buffer));
					else if (r->subnet.ip_ver == IPV6_A)
						res = sprint_uint(buffer, r->subnet.mask);
					else {
						strcpy(buffer, "<Invalid mask>");
						res = strlen(buffer);
					}
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, pad_value);
					j += res;
					break;
				case 'm':
					IPAM_HEADER("mask")
					if (r->subnet.ip_ver == IPV4_A || r->subnet.ip_ver == IPV6_A)
						res = sprint_uint(buffer, r->subnet.mask);
					else {
						strcpy(buffer, "<Invalid mask>");
						res = strlen(buffer);
					}
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'D':
					IPAM_HEADER("device")
					res = strlen(r->device);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, r->device,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'C':
					IPAM_HEADER("comment")
					if (r->ea[0].value == NULL) {
						buffer[0] = '\0';
						res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							0, field_width, pad_left, ' ');
					} else {
						res = strlen(r->ea[0].value);
						res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, r->ea[0].value,
							res, field_width, pad_left, ' ');
					}
					j += res;
					break;
				case 'U': /* upper subnet */
				case 'L': /* lower subnet */
				case 'I': /* IP address */
				case 'B': /* last IP Address of the subnet */
				case 'N': /* network adress of the subnet */
					IPAM_HEADER("prefix")
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
					IPAM_HEADER("prefix")
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					subnet2str(&r->subnet, buffer2, sizeof(buffer2), compression_level);
					res = sprintf(buffer, "%s/%d", buffer2, (int)r->subnet.mask);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'G':
					IPAM_HEADER("GW")
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					copy_ipaddr(&sub.ip_addr, &r->gw);
					sub.ip_ver = r->subnet.ip_ver;
					res = subnet2str(&sub, buffer, sizeof(buffer), compression_level);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'O': /* Extended Attribute */
					ea_num = 0;
					if (fmt[i + 2] == '#') {
						int k;
						char sep = fmt[i + 3];
						if (sep == '\0') /* set the default separator */
							sep = ';';
						for (k = 0; k < r->ea_nr; k++) {
							if (header)
								res = strxcpy(buffer,
										r->ea[k].name,
										sizeof(buffer));
							else {
								if (r->ea[k].value == NULL) {
									buffer[0] = '\0';
									res = 0;
								} else
									res = strxcpy(buffer,
											r->ea[k].value,
											sizeof(buffer));
							}
							res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j,
									buffer, res, field_width, pad_left, ' ');
							j += res;
							if (k != r->ea_nr - 1) {
								outbuf[j] = sep;
								j++;
							}
						}
						i++;
						break;
					}
					while (isdigit(fmt[i + 2])) {
						ea_num *= 10;
						ea_num += fmt[i + 2] - '0';
						i++;
					}
					if (ea_num >= r->ea_nr) {
						debug(FMT, 3, "Invalid Extended Attribute number #%d, max %d\n",							 ea_num, r->ea_nr);
						break;
					}
					if (header)
						res = strxcpy(buffer, r->ea[ea_num].name, sizeof(buffer));
					else {
						if (r->ea[ea_num].value == NULL) {
							buffer[0] = '\0';
							res = 0;
						} else
							res = strxcpy(buffer, r->ea[ea_num].value,
									sizeof(buffer));
					}
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				default:
					debug(FMT, 2, "%c is not a valid char after a %c\n", fmt[i2], '%');
					outbuf[j] = '%';
					outbuf[j + 1] = fmt[i2];
					j += 2;
			} /* switch */
			i += 2;
		} else if (c == '\\') {
			BLOCK_ESCAPE_CHAR
		} else {
			outbuf[j++] = c;
			i++;
		}
	}
	outbuf[j++] = '\n';
	outbuf[j] = '\0';
	return fputs(outbuf, output);
}

int fprint_route_fmt(FILE *output, const struct route *r, const char *fmt) {
	return __fprint_route_fmt(output, r, fmt, 0);
}

int fprint_route_header(FILE *output, const struct route *r, const char *fmt) {
	return __fprint_route_fmt(output, r, fmt, 1);
}

static int __fprint_ipam_fmt(FILE *output, const struct ipam_line *r,
		const char *fmt, int header) {
	int i, j, i2, compression_level;
	int res, pad_left;
	char c;
	char outbuf[512 + 140];
	char buffer[128], buffer2[128];
	int field_width, ea_num;
	struct subnet v_sub;
	char pad_value;
	/* %I for IP */
	/* %m for mask */
	/* %00....9 for extanded attributes */
	i = 0;
	j = 0; /* index in outbuf */
	while (1) {
		c = fmt[i];
		debug(FMT, 5, "Still to parse : '%s'\n", fmt + i);
		if (j >= sizeof(outbuf)) {
			fprintf(stderr, "BUG in %s, buffer overrun, j=%d len=%d\n", __FUNCTION__, j,
					 (int)sizeof(outbuf));
			break;
		} else if (j == sizeof(outbuf) - 1) {
			debug(FMT, 2, "Output buffer is full, stopping\n");
			break;
		}
		if (c == '\0')
			break;
		if (c == '%') {
			BLOCK_FIELD_WIDTH
			pad_value = ' '; /*in fprint_route, pad value is alwys a space */
			switch (fmt[i2]) {
				case '\0':
					outbuf[j] = '%';
					debug(FMT, 2, "End of String after a %c\n", '%');
					j++;
					i--;
					break;
				case 'M':
					IPAM_HEADER("mask")
					if (r->subnet.ip_ver == IPV4_A)
						res = mask2ddn(r->subnet.mask, buffer, sizeof(buffer));
					else if (r->subnet.ip_ver == IPV6_A)
						res = sprint_uint(buffer, r->subnet.mask);
					else {
						strcpy(buffer, "<Invalid mask>");
						res = strlen(buffer);
					}
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, pad_value);
					j += res;
					break;
				case 'm':
					IPAM_HEADER("mask")
					if (r->subnet.ip_ver == IPV4_A || r->subnet.ip_ver == IPV6_A)
						res = sprint_uint(buffer, r->subnet.mask);
					else {
						strcpy(buffer, "<Invalid mask>");
						res = strlen(buffer);
					}
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'I': /* IP address */
					IPAM_HEADER("address")
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					copy_subnet(&v_sub, &r->subnet);
					res = subnet2str(&v_sub, buffer, sizeof(buffer), compression_level);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'P': /* Prefix */
					IPAM_HEADER("prefix")
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					copy_subnet(&v_sub, &r->subnet);
					subnet2str(&v_sub, buffer2, sizeof(buffer2), compression_level);
					res = sprintf(buffer, "%s/%d", buffer2, (int)v_sub.mask);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'O': /* Extended Attribute */
					ea_num = 0;
					if (fmt[i + 2] == '#') {
						int k;
						char sep = fmt[i + 3];
						if (sep == '\0') /* set the default separator */
							sep = ';';
						for (k = 0; k < r->ea_nr; k++) {
							if (header)
								res = strxcpy(buffer,
										r->ea[k].name,
										sizeof(buffer));
							else {
								if (r->ea[k].value == NULL) {
									buffer[0] = '\0';
									res = 0;
								} else
									res = strxcpy(buffer,
											r->ea[k].value,
											sizeof(buffer));
							}
							res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j,
									buffer, res, field_width, pad_left, ' ');
							j += res;
							if (k != r->ea_nr - 1) {
								outbuf[j] = sep;
								j++;
							}
						}
						i++;
						break;
					}
					while (isdigit(fmt[i + 2])) {
						ea_num *= 10;
						ea_num += fmt[i + 2] - '0';
						i++;
					}
					if (ea_num >= r->ea_nr) {
						debug(FMT, 3, "Invalid Extended Attribute number #%d, max %d\n",							 ea_num, r->ea_nr);
						break;
					}
					if (header)
						res = strxcpy(buffer, r->ea[ea_num].name, sizeof(buffer));
					else
						res = strxcpy(buffer, r->ea[ea_num].value, sizeof(buffer));
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				default:
					debug(FMT, 2, "%c is not a valid char after a %c\n", fmt[i2], '%');
					outbuf[j] = '%';
					outbuf[j + 1] = fmt[i2];
					j += 2;
			} /* switch */
			i += 2;
		} else if (c == '\\') {
			BLOCK_ESCAPE_CHAR
		} else {
			outbuf[j++] = c;
			i++;
		}
	}
	outbuf[j++] = '\n';
	outbuf[j] = '\0';
	return fputs(outbuf, output);
}

int fprint_ipam_fmt(FILE *output, const struct ipam_line *r, const char *fmt) {
	return __fprint_ipam_fmt(output, r, fmt, 0);
}

int fprint_ipam_header(FILE *output, const struct ipam_line *r, const char *fmt) {
	int i, a = 0;

	if (strlen(fmt) < 2) {
		a += fprintf(output, "prefix;");
		for (i = 0; i < r->ea_nr; i ++)
			a += fprintf(output, "%s;", r->ea[i].name);
		a += fprintf(output, "\n");
		return a;
	}
	return __fprint_ipam_fmt(output, r, fmt, 1);
}
/*
 * a very specialized function to print a struct bgp_route */
int fprint_bgproute_fmt(FILE *output, const struct bgp_route *r, const char *fmt) {
	int i, j, i2, compression_level;
	int res, pad_left;
	char c;
	char outbuf[512 + 140];
	char buffer[128], buffer2[128];
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
		if (j >= sizeof(outbuf)) {
			fprintf(stderr, "BUG in %s, buffer overrun, j=%d len=%d\n", __FUNCTION__, j,
					 (int)sizeof(outbuf));
			break;
		} else if (j == sizeof(outbuf) - 1) {
			debug(FMT, 2, "Output buffer is full, stopping\n");
			break;
		}
		if (c == '\0')
			break;
		if (c == '%') {
			BLOCK_FIELD_WIDTH
			pad_value = ' '; /*in fprint_bgp_route, pad value is alwys a space */
			switch (fmt[i2]) {
				case '\0':
					outbuf[j] = '%';
					debug(FMT, 2, "End of String after a %c\n", '%');
					j++;
					i--;
					break;
				case 'w':
					PRINT_FILE_HEADER(WEIGHT)
					res = sprint_uint(buffer, r->weight);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, pad_value);
					j += res;
					break;
				case 'L':
					PRINT_FILE_HEADER(LOCAL_PREF)
					res = sprint_uint(buffer, r->LOCAL_PREF);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, pad_value);
					j += res;
					break;
				case 'A':
					PRINT_FILE_HEADER(AS_PATH)
					res = strlen(r->AS_PATH);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, r->AS_PATH,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'M':
					PRINT_FILE_HEADER(MED)
					res = sprint_uint(buffer, r->MED);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, pad_value);
					j += res;
					break;
				case 'o':
					PRINT_FILE_HEADER(ORIGIN)
					buffer[0] = r->origin;
					buffer[1] = '\0';
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							1, field_width, pad_left, pad_value);
					j += res;
					break;
				case 'b':
					PRINT_FILE_HEADER(BEST)
					truc = (r->best ? "1" : "0");
					res = strlen(truc);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, truc,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'B':
					PRINT_FILE_HEADER(BEST)
					truc = (r->best ? "Best" : "No");
					res = strlen(truc);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, truc,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'v':
					PRINT_FILE_HEADER(V)
					outbuf[j] = (r->valid ? '1' : '0');
					j++;
					break;
				case 'T':
					PRINT_FILE_HEADER(Proto)
					truc = (r->type == 'i'? "iBGP" : "eBGP");
					res = strlen(truc);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, truc,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'm':
					PRINT_FILE_HEADER(Mask)
					if (r->subnet.ip_ver == IPV4_A || r->subnet.ip_ver == IPV6_A)
						res = sprint_uint(buffer, r->subnet.mask);
					else {
						strcpy(buffer, "<Invalid mask>");
						res = strlen(buffer);
					}
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'I': /* IP address */
					PRINT_FILE_HEADER(IP)
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					copy_subnet(&v_sub, &r->subnet);
					res = subnet2str(&v_sub, buffer, sizeof(buffer), compression_level);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'P': /* Prefix */
					PRINT_FILE_HEADER(Prefix)
					SET_IP_COMPRESSION_LEVEL(fmt[i2 + 1]);
					copy_subnet(&v_sub, &r->subnet);
					subnet2str(&v_sub, buffer2, sizeof(buffer2), compression_level);
					res = sprintf(buffer, "%s/%d", buffer2, (int)v_sub.mask);
					res = pad_buffer_out(outbuf + j, sizeof(outbuf) - j, buffer,
							res, field_width, pad_left, ' ');
					j += res;
					break;
				case 'G':
					PRINT_FILE_HEADER(GW)
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
			} /* switch */
			i += 2;
		} else if (c == '\\') {
			BLOCK_ESCAPE_CHAR
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
	char buffer[256];
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
			pad_value = ' ';
			BLOCK_FIELD_WIDTH
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
						strcpy(buffer,"<Invalid mask>");
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
						strcpy(buffer,"<Invalid mask>");
						res = strlen(buffer);
					}
					res = pad_buffer_out(outbuf + j, len - j, buffer, res,
							field_width, pad_left, ' ');
					j += res;
					break;
				case 's': /* almost standard %s printf, except that size is limited to 128 */
					v_s = va_arg(ap, char *);
					res = strxcpy(buffer, v_s, sizeof(buffer) - 1);

					if (strlen(v_s) >= sizeof(buffer) - 1)
						debug(FMT, 3, "truncating string '%s' to %d bytes\n", v_s, (int)(sizeof(buffer) - 1));
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
				case 'h':/* half integers handling */
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
						debug(FMT, 2, "Invalid format '%c' after '%%h'\n", fmt[i2 + 1]);
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
						debug(FMT, 2, "Invalid format '%c' after '%%l'\n", fmt[i2 + 1]);
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
				case 'O': /* st_object */
					if (o == NULL)
						break;
					o_num = 0;
					while (isdigit(fmt[i + 2])) {
						o_num *= 10;
						o_num += fmt[i + 2] - '0';
						i++;
					}
					if (o_num >= max_o) {
						debug(FMT, 3, "Invalid object number #%d, max %d\n", o_num, max_o);
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
			} /* switch */
			i += 2;
		} else if (c == '\\') {
			BLOCK_ESCAPE_CHAR
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
	return fputs(buffer, f);
}

int st_printf(const char *fmt, ...) {
	char buffer[ST_VSPRINTF_BUFFER_SIZE];
	va_list ap;

	va_start(ap, fmt);
	st_vsnprintf(buffer, sizeof(buffer), fmt, ap, NULL, 0);
	va_end(ap);
	return fputs(buffer, stdout);
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

void fprint_subnet_file(FILE *output, const struct subnet_file *sf, int compress_level) {
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		fprint_route(output, &sf->routes[i], compress_level);
}

void fprint_subnet_file_fmt(FILE *output, const struct subnet_file *sf, const char *fmt) {
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		fprint_route_fmt(output, &sf->routes[i], fmt);
}

void print_subnet_file(const struct subnet_file *sf, int compress_level) {
	fprint_subnet_file(stdout, sf, compress_level);
}

void fprint_bgp_file_fmt(FILE *output, const struct bgp_file *sf, const char *fmt) {
	unsigned long i;

	for (i = 0; i < sf->nr; i++)
		fprint_bgproute_fmt(output, &sf->routes[i], fmt);
}

void print_bgp_file_fmt(const struct bgp_file *sf, const char *fmt) {
	fprint_bgp_file_fmt(stdout, sf, fmt);
}

static void fprint_ipam_file(FILE *out, const struct ipam_file *sf) {
	int i, j;

	for (i = 0; i < sf->nr; i++) {
		st_fprintf(out, "%P;", sf->lines[i].subnet);
		for (j = 0; j < sf->ea_nr; j++)
			fprintf(out, "%s;", sf->lines[i].ea[j].value);
		fprintf(out, "\n");
	}
}

void fprint_ipam_file_fmt(FILE *output, const struct ipam_file *sf, const char *fmt) {
	unsigned long i;

	/* if user didnt provide a fmt, just use the simple fprint_ipam_file */
	if (strlen(fmt) < 2)
		return fprint_ipam_file(output, sf);
	for (i = 0; i < sf->nr; i++)
		fprint_ipam_fmt(output, &sf->lines[i], fmt);
}

void print_ipam_file_fmt(const struct ipam_file *sf, const char *fmt) {
	fprint_ipam_file_fmt(stdout, sf, fmt);
}
