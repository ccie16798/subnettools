/*
 * IPv4, IPv6 subnet/routes scanf equivalent with PATTERN matching
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
#include "utils.h"
#include "iptools.h"
#include "debug.h"
#include "st_object.h"


snprint_signed(short)
snprint_signed(int)
snprint_signed(long)
snprint_hex(short)
snprint_hex(int)
snprint_hex(long)
snprint_unsigned(short)
snprint_unsigned(int)
snprint_unsigned(long)


int sto_is_string(struct sto *o) {
	return (o->type == 's' || o->type == 'S' || o->type == 'W' || o->type == '[');
}

int sto2string(char *s, const struct sto *o, size_t len, int comp_level) {
	char buffer[128];
	int res;

	switch (o->type) {
		case '[':
		case 's':
		case 'W':
		case 'S':
			return strxcpy(s, o->s_char, len);
			break;
		case 'I':
			return addr2str(&o->s_addr, s, len, comp_level);
			break;
		case 'Q':
		case 'P':
			subnet2str(&o->s_subnet, buffer, sizeof(buffer), comp_level);
			return snprintf(s, len, "%s/%d", buffer, (int)o->s_subnet.mask);
			break;
		case 'x':
			if (o->conversion == 'l')
				res = snprint_hexlong(s, o->s_ulong, len - 1);
			else if (o->conversion == 'h')
				res = snprint_hexshort(s, o->s_ushort, len - 1);
			else
				res = snprint_hexint(s, o->s_uint, len - 1);
			s[res] = '\0';
			return res;
			break;
		case 'u':
			if (o->conversion == 'l')
				res = snprint_ulong(s, o->s_ulong, len - 1);
			else if (o->conversion == 'h')
				res = snprint_ushort(s, o->s_ushort, len - 1);
			else
				res = snprint_uint(s, o->s_uint, len - 1);
			s[res] = '\0';
			return res;
			break;
		case 'd':
			if (o->conversion == 'l')
				res = snprint_long(s, o->s_long, len - 1);
			else if (o->conversion == 'h')
				res = snprint_short(s, o->s_short, len - 1);
			else
				res = snprint_int(s, o->s_int, len - 1);
			s[res] = '\0';
			return res;
			break;
		case 'M':
			res = snprint_int(s, o->s_int, len - 1);
			s[res] = '\0';
			return res;
			break;
		case 'l':
			res = snprint_long(s, o->s_long, len - 1);
			s[res] = '\0';
			return res;
			break;
		case 'c':
			s[0] = o->s_char[0];
			s[1] = '\0';
			return 1;
			break;
		default:
			s[0] = '\0';
			break;
	}
	return -1;
}
