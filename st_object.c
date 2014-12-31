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


int sto2string(char *s, const struct sto *o, size_t len, int comp_level) {
	char buffer[128];
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
		case 'P':
			subnet2str(&o->s_subnet, buffer, sizeof(buffer), comp_level);
			return snprintf(s, len, "%s/%d", buffer, (int)o->s_subnet.mask);
			break;
		case 'd':
		case 'M':
			return snprintf(s, len, "%d", o->s_int);
			break;
		case 'l':
			return snprintf(s, len, "%ld", o->s_long);
			break;
		case 'c':
			s[0] = o->s_char[0];
			s[1] = '\0';
			return 1;
			break;
		default:
			s[0] = '\0';
			return -1;
			break;
	}
}
