/*
 * IPv4, IPv6 subnet/routes scanf equivalent with PATTERN matching
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
#include <stdarg.h>
#include <ctype.h>
#include "utils.h"
#include "iptools.h"
#include "debug.h"
#include "st_scanf.h"
#include "st_object.h"

#define ST_STRING_INFINITY 1000000000  /* Subnet tool definition of infinity */

struct expr {
	 /* used to break '.*' expansion */
	int (*can_stop)(const char *remain, struct expr *e);
	char end_of_expr; /* if remain[i] = end_of_expr , we can stop*/
	char end_expr[64];
	int end_expr_len; /* length of 'fmt' we want to sopt on */
	int can_skip; /* number of char we can skip in next iteration */
	int skip_on_return; /* number of char we can skip when '.*' exp finishes' */
	struct sto sto[10]; /* object collected by find_xxx */
	int num_o; /* number of object collected by find_xxxx*/
};


/* return the escaped char */
static inline char escape_char(char c)
{
	switch (c) {
	case '\0':
		debug(SCANF, 2, "Nul char after Escape Char '\\'\n");
		return '\0';
	case 't':
		return '\t';
	case 'n':
		return '\n';
	default:
		return c;
	}
}

static inline int is_multiple_char(char c)
{
	return (c == '*' || c == '+' || c == '?' || c == '{');
}

static inline int max_match(char c)
{
	if (c == '*')
		return ST_STRING_INFINITY;
	if (c == '+')
		return ST_STRING_INFINITY;
	if (c == '?')
		return 1;
	debug(SCANF, 1, "BUG, Invalid multiplier char '%c'\n", c);
	return 0;
}

static inline int min_match(char c)
{
	if (c == '*')
		return 0;
	if (c == '+')
		return 1;
	if (c == '?')
		return 0;
	debug(SCANF, 1, "BUG, Invalid multiplier char '%c'\n", c);
	return 0;
}

/*
 * parse a brace multiplier like {,1} or {2} or {3,4} {4,}
 * min & max are set by this helper
 * returns :
 *	-1 if string is invalid
 *	number of matched chars
 */
static int parse_brace_multiplier(const char *s, int *min, int *max)
{
	int i = 1;

	*min = 0;
	*max = ST_STRING_INFINITY;
	if (s[i] == '}') {
		debug(SCANF, 1, "Invalid empty multiplier '%s'\n", s);
		return -1;
	}
	while (isdigit(s[i])) {
		*min *= 10;
		*min += s[i] - '0';
		i++;
	}
	if (s[i] == ',') {
		i++;
		if (s[i] == '}') {
			return i;
		} else if (isdigit(s[i])) {
			*max = 0;
			while (isdigit(s[i])) {
				*max *= 10;
				*max += s[i] - '0';
				i++;
			}
			if (s[i] != '}') {
				debug(SCANF, 1, "Invalid range '%s', no closing '}'\n", s);
				return -1;
			}
			if (*min > *max) {
				debug(SCANF, 1, "Invalid range, min:%d > max:%d\n", *min, *max);
				return -1;
			}
			return i;
		}
		debug(SCANF, 1, "Invalid range '%s' contains invalid char '%c'\n", s, s[i]);
		return -1;
	} else if (s[i] == '}') {
		*max = *min;
		return i;
	}
	debug(SCANF, 1, "Invalid range '%s' contains invalid char '%c'\n", s, s[i]);
	return -1;
}
/* count number of consersion specifier in an expr
 * doesnt validate CS are valid
 */
static inline int count_cs(const char *expr)
{
	int i;
	int n = 0;

	if (expr[0] == '%')
		n++;
	for (i = 1; ; i++) {
		if (expr[i] == '\0')
			return n;
		/* unlike regular scanf, escape char is '\' and only that */
		if (expr[i] == '%' && expr[i - 1] != '\\')
			n++;
	}
	return 0;
}

/* given an expression e, the following function
 * try to determine if the remaining chars match their type
 * for exemple, find_int will match on the first digit found
*/
static int find_int(const char *remain, struct expr *e)
{
	return isdigit(*remain) || (*remain == '-' && isdigit(remain[1]));
}

static int find_uint(const char *remain, struct expr *e)
{
	return isdigit(*remain);
}

static int find_word(const char *remain, struct expr *e)
{
	return isalpha(*remain);
}

static int find_string(const char *remain, struct expr *e)
{
	return !isspace(*remain);
}

static int find_hex(const char *remain, struct expr *e)
{
	if (remain[0] == '0' && remain[1] == 'x' && isxdigit(remain[2]))
		return 1;
	return isxdigit(*remain);
}

/*
 * from fmt string starting with '[', fill expr with the range
 * a bit like strxpy_until, but not quite since a ']' is allowed right
 * after the opening '[' or a '^' if present
 * @expr  : the buffer to fill
 * @fmt   : the format string (fmt[0] ) '[' in this function)
 * @n     : max number of char to store in expr (including NUL)
 * returns:
 *     strlen(expr) on SUCCESS
 *     -1 if fmt is badly formatted (no closing ']')
 */
static int fill_char_range(char *expr, const char *fmt, int n)
{
	int i = 1;

	/* to include a ']' in a range, it must be right after the opening ']' or after '^'
	 * we need to copy it before we enter the generic loop
	 */
	expr[0] = '[';
	if (fmt[i] == '^') {
		expr[i] = '^';
		i++;
	}
	if (fmt[i] == ']') {
		expr[i] = ']';
		i++;
	}

	while (fmt[i] != ']') {
		if (fmt[i] == '\0' || i == n - 2)
			return -1;
		expr[i] = fmt[i];
		i++;
	}
	expr[i] = ']';
	expr[i + 1] = '\0';
	return i + 1;
}

/*
 * match character c against STRING 'expr' like [acde-g]
 * @c    : the char to match
 * @expr : a pointer to the expr to test c against, pointer will be updated
 * returns :
 *    1 if a match is found
 *    0 if no match
 *    -1 if range is invalid
 */
static int match_char_against_range(char c, const char **expr)
{
	int res = 0;
	char low, high;
	int invert = 0;

	*expr += 1;
	if (**expr == '^') {
		invert = 1;
		*expr += 1;
	}
	/* to include a ']' in a range, must be right after '[' of '[^' */
	if (**expr == ']') {
		res = (c == ']');
		*expr += 1;
	}

	while (**expr != ']') {
		low = **expr;
		if (low == '\0') {
			debug(SCANF, 1, "Invalid expr '%s', no closing ']' found\n", *expr);
			return -1;
		}
		/* expr[*i + 2] != ']' means we can match a '-' only if it is right
		 * before the ending ']'
		 */
		if ((*expr)[1] == '-' && (*expr)[2] != ']') {
			high = (*expr)[2];
			if (high == '\0') {
				debug(SCANF, 1, "Invalid expr '%s', incomplete range\n", *expr);
				return -1;
			}
			if (c >= low && c <= high)
				res = 1;
			*expr += 1;
		} else {
			if (low == c)
				res = 1;
		}
		*expr += 1;
	}
	*expr += 1;
	return invert ? !res : res;
}


/* match character c against STRING 'expr' like [acde-g]
 * We KNOW expr is a valid char range, no bound checking needed
 * @c    : the char to match
 * @expr : the expr to test c against
 * returns :
 *    1 if a match is found
 *    0 if no match
 */
static int match_char_against_range_clean(char c, const char *expr)
{
	int res = 0;
	char low, high;
	int invert = 0;

	expr += 1;
	if (*expr == '^') {
		invert = 1;
		expr += 1;
	}
	/* to include a ']' in a range, must be right after '[' of '[^' */
	if (*expr == ']') {
		res = (c == ']');
		expr += 1;
	}

	while (*expr != ']') {
		low = *expr;
		/* expr[*i + 2] != ']' means we can match a '-' only if it is right
		 * before the ending ']'
		 */
		if (expr[1] == '-' && expr[2] != ']') {
			high = expr[2];
			if (c >= low && c <= high)
				res = 1;
			expr += 1;
		} else {
			if (low == c)
				res = 1;
		}
		expr += 1;
	}
	return invert ? !res : res;
}

/* parse input STRING 'in' according to format 'fmt'
 * **fmt == '%' when the function starts (conversion specifier)
 * store output in o if not NULL, else put it to thrash
 * @in  : pointer to remaining input buffer
 * @fmt : pointer to remaining FORMAT buffer
 * @o   : a struct to store a found objet; if NULL, discard found data
 * returns:
 *	the number of conversion specifiers found (0 or 1)
*/
static int parse_conversion_specifier(const char **in, const char **fmt,
		struct sto *o)
{
	int n_found = 0; /* number of CS found */
	int i2, res;
	int max_field_length;
	char buffer[128];
	char *ptr_buff;
	char poubelle[256];
	char c;
	struct subnet *v_sub;
	char expr[128];
	struct ip_addr *v_addr;
	char *v_s;
	long *v_long;
	int *v_int;
	short *v_short;
	unsigned int *v_uint;
	int sign;
	const char *p, *p_max;

#define ARG_SET(__NAME, __TYPE) \
	do { \
		if (o == NULL) \
			__NAME = (__TYPE)&poubelle; \
		else { \
			__NAME = (__TYPE)&o->s_char; \
			o->type = (*fmt)[1]; \
			o->conversion = 0; \
		} \
	} while (0)

	p = *in;
	/* computing field length */
	if (isdigit((*fmt)[1])) {
		max_field_length = (*fmt)[1] - '0';
		*fmt += 2;
		while (isdigit(**fmt)) {
			max_field_length *= 10;
			max_field_length += **fmt - '0';
			*fmt += 1;
		}
		*fmt -= 1;
		if (max_field_length > sizeof(buffer) - 2)
			max_field_length = sizeof(buffer) - 2;
		debug(SCANF, 9, "Found max field length %d\n", max_field_length);
	} else
		max_field_length = sizeof(buffer) - 2;
	p_max = *in + max_field_length - 1;
	c = (*fmt)[1];
	switch (c) {
	case '\0':
		debug(SCANF, 1, "Invalid format '%s', ends with %%\n", *fmt);
		return n_found;
	case 'Q': /* classfull subnet */
	case 'P':
		ARG_SET(v_sub, struct subnet *);
		ptr_buff = buffer;
		while ((is_ip_char(*p) || *p == '/')) {
			*ptr_buff = *p;
			p++;
			ptr_buff++;
		}
		*ptr_buff = '\0';
		if (p - *in <= 2) {
			debug(SCANF, 3, "no IP found at %s\n", *in);
			return n_found;
		}
		if ((*fmt)[1] == 'P')
			res = get_subnet_or_ip(buffer, v_sub);
		else
			res = classfull_get_subnet(buffer, v_sub);
		if (res > 0) {
			debug(SCANF, 5, "'%s' is a valid IP\n", buffer);
			n_found++;
		} else {
			debug(SCANF, 3, "'%s' is an invalid IP\n", buffer);
			return n_found;
		}
		break;
	case 'I':
		ARG_SET(v_addr, struct ip_addr *);
		while (is_ip_char(*p))
			p++;
		if (p - *in <= 1) {
			debug(SCANF, 3, "no IP found\n");
			return n_found;
		}
		res = string2addr(*in, v_addr, p - *in);
		if (res > 0) {
			debug(SCANF, 5, "valid IP found at %s\n", *in);
			n_found++;
		} else {
			debug(SCANF, 3, "invalid IP at %s\n", *in);
			return n_found;
		}
		break;
	case 'M':
		ARG_SET(v_int, int *);
		while (isdigit(*p) || *p == '.')
			p++;
		if (p - *in == 0) {
			debug(SCANF, 3, "no MASK found at %s\n", *in);
			return n_found;
		}
		res = string2mask(*in, p - *in);
		if (res != BAD_MASK) {
			debug(SCANF, 5, "Valid MASK found at %s\n", *in);
			n_found++;
		} else {
			debug(SCANF, 3, "Invalid MASK at %s\n", *in);
			return n_found;
		}
		*v_int = res;
		break;
	case 'h':
		*fmt += 1;
		c = (*fmt)[1];
		if (c != 'd' && c != 'u' && c != 'x') {
			debug(SCANF, 1, "Invalid format '%s', wrong char '%c' after 'h'\n",
					*fmt, c);
			return n_found;
		}
		ARG_SET(v_short, short *);
		if (o)
			o->conversion = 'h';
		*v_short = 0;
		if (c == 'x') {
			if (*p == '0' && p[1] == 'x')
				p += 2;
			if (!isxdigit(*p)) {
				debug(SCANF, 3, "no HEX found at %s\n", *in);
				return n_found;
			}
			while (isxdigit(*p)) {
				*v_short <<= 4;
				*v_short += char2int(*p);
				p++;
			}
			debug(SCANF, 5, "found short HEX '%x' at %s\n",
					*v_short, *in);
		} else {
			if (*p == '-' && c == 'd') {
				sign = -1;
				p++;
			} else
				sign = 1;
			if (!isdigit(*p)) {
				debug(SCANF, 3, "no SHORT found at %s\n", *in);
				return n_found;
			}
			while (isdigit(*p)) {
				*v_short *= 10;
				*v_short += (*p - '0');
				p++;
			}
			*v_short *= sign;
			if (c == 'u') {
				debug(SCANF, 5, "found USHORT '%hu' at %s\n",
						(unsigned short)*v_short, *in);
			} else {
				debug(SCANF, 5, "found SHORT '%hd' at %s\n",
						*v_short, *in);
			}
		}
		n_found++;
		break;
	case 'l':
		*fmt += 1;
		c = (*fmt)[1];
		if (c != 'd' && c != 'u' && c != 'x') {
			debug(SCANF, 1, "Invalid format, wrong char '%c' after 'l'\n", c);
			return n_found;
		}
		ARG_SET(v_long, long *);
		if (o)
			o->conversion = 'l';
		*v_long = 0;
		if (c == 'x') {
			if (*p == '0' && p[1] == 'x')
				p += 2;
			if (!isxdigit(*p)) {
				debug(SCANF, 3, "no HEX found at %s\n", *in);
				return n_found;
			}
			while (isxdigit(*p)) {
				*v_long <<= 4;
				*v_long += char2int(*p);
				p++;
			}
			debug(SCANF, 5, "found long HEX '%lx' at %s\n",
					(unsigned long)*v_long, *in);
		} else {
			if (*p == '-' && c == 'd') {
				sign = -1;
				p++;
			} else
				sign = 1;
			if (!isdigit(*p)) {
				debug(SCANF, 3, "no LONG found at %s\n", *in);
				return n_found;
			}
			while (isdigit(*p)) {
				*v_long *= 10UL;
				*v_long += (*p - '0');
				p++;
			}
			*v_long *= sign;
			if (c == 'u') {
				debug(SCANF, 5, "found ULONG '%lu' at %s\n",
						(unsigned long)*v_long, *in);
			} else {
				debug(SCANF, 5, "found LONG '%ld' at %s\n",
						*v_long, *in);
			}
		}
		n_found++;
		break;
	case 'd':
		ARG_SET(v_int, int *);
		*v_int = 0;
		if (*p == '-') {
			sign = -1;
			p++;
		} else
			sign = 1;
		if (!isdigit(*p)) {
			debug(SCANF, 3, "no INT found at %s\n", *in);
			return n_found;
		}
		while (isdigit(*p)) {
			*v_int *= 10;
			*v_int += (*p - '0');
			p++;
		}
		*v_int *= sign;
		debug(SCANF, 5, "found INT '%d' at %s\n", *v_int, *in);
		n_found++;
		break;
	case 'u':
		ARG_SET(v_uint, unsigned int *);
		*v_uint = 0;
		if (!isdigit(*p)) {
			debug(SCANF, 3, "no UINT found at %s\n", *in);
			return n_found;
		}
		while (isdigit(*p)) {
			*v_uint *= 10;
			*v_uint += (*p - '0');
			p++;
		}
		debug(SCANF, 5, "found UINT '%u' at %s\n", *v_uint, *in);
		n_found++;
		break;
	case 'x':
		ARG_SET(v_uint, unsigned int *);
		*v_uint = 0;
		if (*p == '0' && p[1] == 'x')
			p += 2;
		if (!isxdigit(*p)) {
			debug(SCANF, 3, "no HEX found at %s\n", *in);
			return n_found;
		}
		while (isxdigit(*p)) {
			*v_uint <<= 4;
			*v_uint += char2int(*p);
			p++;
		}
		debug(SCANF, 5, "found HEX '%x' at %s\n", *v_uint, *in);
		n_found++;
		break;
	case 'S': /* a special STRING that doesnt represent an IP */
	case 's':
		ARG_SET(v_s, char *);
		c = (*fmt)[2];
		if (c == '.') {
			debug(SCANF, 1, "Invalid format, found '.' after %%s\n");
			return n_found;
		} else if (c == '%') {
			debug(SCANF, 1, "Invalid format, found '%%' after %%s\n");
			return n_found;
		}
		ptr_buff = v_s;
		while (!isspace(*p) && p < p_max) {
			if (*p == '\0')
				break;
			*ptr_buff = *p;
			p++;
			ptr_buff++;
		}
		if (p == *in) {
			debug(SCANF, 3, "no STRING found at %s\n", *in);
			return n_found;
		}
		*ptr_buff = '\0';
		if ((*fmt)[1] == 'S') {
			res = get_subnet_or_ip(v_s, (struct subnet *)&poubelle);
			if (res > 0) {
				debug(SCANF, 3, "STRING '%s' is an IP\n", v_s);
				return n_found;
			}
		}
		debug(SCANF, 5, "found STRING '%s'\n", v_s);
		n_found++;
		break;
	case 'W':
		ARG_SET(v_s, char *);
		ptr_buff = v_s;
		while (isalpha(*p) && p < p_max) {
			*ptr_buff = *p;
			p++;
			ptr_buff++;
		}
		if (p == *in) {
			debug(SCANF, 3, "no WORD found at %s\n", *in);
			return 0;
		}
		*ptr_buff = '\0';
		debug(SCANF, 5, "WORD '%s' found\n", v_s);
		n_found++;
		break;
	case '[':
		ARG_SET(v_s, char *);
		i2 = fill_char_range(expr, (*fmt) + 1, sizeof(expr));
		if (i2 == -1) {
			debug(SCANF, 1, "Invalid format '%s', no closing ']'\n", *fmt);
			return n_found;
		}
		*fmt += (i2 - 1);
		i2 = 0;
		ptr_buff = v_s;
		while (match_char_against_range_clean(*p, expr) && p < p_max) {
			if (*p == '\0')
				break;
			*ptr_buff = *p;
			p++;
			ptr_buff++;
		}
		if (p == *in) {
			debug(SCANF, 3, "no CHAR RANGE found at %s\n", *in);
			return 0;
		}
		*ptr_buff = '\0';
		debug(SCANF, 5, "CHAR RANGE '%s' found at %s\n", v_s,  *in);
		n_found++;
		break;
	case 'c':
		ARG_SET(v_s, char *);
		v_s[0] = *p;
		debug(SCANF, 5, "CHAR '%c'\n", *v_s);
		p++;
		n_found++;
		break;
	default:
		debug(SCANF, 1, "Unknown conversion specifier '%c'\n", c);
		break;
	} /* switch */
	*in = p;
	*fmt += 2;
	return n_found;
}

/*
 * match a single pattern 'expr' against 'in'
 * @expr  : the expression to match
 * @in    : the input buffer
 * @o     : will store input data (if conversion specifiers are found)
 * @num_o : number of found objects (will be updated)
 * returns:
 *  0 if it doesnt match,
 *  number of matched chars in input buffer if it matches
 */
static int match_expr_single(const char *expr, const char *in, struct sto *o, int *num_o)
{
	int res;
	char c;
	char *p;
	const char *saved_in;
	int saved_num_o = *num_o;

	saved_in = in;
	while (1) {
		c = *expr;
		if (c == '\0' || c == '|')
			return in - saved_in;
		debug(SCANF, 8, "remaining in  ='%s'\n", in);
		debug(SCANF, 8, "remaining expr='%s'\n", expr);
		switch (c) {
		case '(':
			expr++;
			continue;
		case ')':
			expr++;
			continue;
		case '.':
			expr++;
			in++;
			continue;
		case '[': /* try to handle char range like [a-Zbce-f] */
			res = match_char_against_range(*in, &expr);
			if (res <= 0)
				break;
			in++;
			continue;
		case '%':
			debug(SCANF, 3, "conversion specifier to handle %lu\n",
					(unsigned long)(o + *num_o));
			res = parse_conversion_specifier(&in, &expr, &o[*num_o]);
			if (res == 0)
				break;
			if (o) {
				debug(SCANF, 4, "conv specifier successfull '%c' for %d\n",
						o[*num_o].type, *num_o);
			} else {
				debug(SCANF, 4, "conv specifier successfull\n");
			}
			*num_o += 1;
			continue;
		case '\\':
			expr++;
			c = escape_char(*expr);
			if (c == '\0') {
				debug(SCANF, 1, "Invalid expr '%s', '\\' at end of string\n",
						expr);
				return 0;
			}
		default:
			if (*in != c)
				break;
			expr++;
			in++;
			continue;
		}
		/* we didnt match, but give a chance to try again if an 'OR' is found */
		p = strchr(expr, '|');
		if (p == NULL)
			return 0;
		in = saved_in;
		expr = p + 1;
		*num_o = saved_num_o;
		debug(SCANF, 4, "Logical OR found, trying again on expr '%s'\n", p);
		continue;
	}
}

static int find_not_ip(const char *remain, struct expr *e)
{
	char buffer[64];
	int i = 0;
	struct subnet s;

	if (isspace(remain[0]))
		return 0;

	while (is_ip_char(remain[i]) && i < sizeof(buffer)) {
		buffer[i] = remain[i];
		i++;
	}
	if (i <= 2)
		return 1;
	buffer[i] = '\0';
	if (get_subnet_or_ip(buffer, &s) > 0) {
		/* remain[0...i] represents an IP, so dont try stop checking in that range */
		e->can_skip = i;
		/* we dont set skip_on_return here */
		return 0;
	} else
		return 1;
}

static int find_subnet(const char *remain, struct expr *e)
{
	char buffer[64];
	int i = 0;

	while ((is_ip_char(remain[i]) || remain[i] == '/') && i < sizeof(buffer)) {
		buffer[i] = remain[i];
		i++;
	}
	if (i <= 1)
		return 0;
	buffer[i] = '\0';
	if (get_subnet_or_ip(buffer, &e->sto[0].s_subnet) > 0) {
		e->can_skip = i; /* useful for .$* matching */
		e->skip_on_return = i;
		e->num_o = 1;
		e->sto[0].type = 'P';
		return 1;
	} else
		return 0;
}

static int find_ip(const char *remain, struct expr *e)
{
	int i = 0;

	while (is_ip_char(remain[i]))
		i++;
	if (i <= 1)
		return 0;
	if (string2addr(remain, &e->sto[0].s_addr, i) > 0) {
		e->can_skip = i;
		e->skip_on_return = i;
		e->num_o = 1;
		e->sto[0].type = 'I';
		return 1;
	} else
		return 0;
}

static int find_classfull_subnet(const char *remain, struct expr *e)
{
	char buffer[64];
	int i = 0;

	while ((is_ip_char(remain[i]) || remain[i] == '/') && i < sizeof(buffer)) {
		buffer[i] = remain[i];
		i++;
	}
	if (i <= 2)
		return 0;
	buffer[i] = '\0';
	if (classfull_get_subnet(buffer, &e->sto[e->num_o].s_subnet) > 0) {
		e->can_skip = i;
		e->skip_on_return = i;
		e->num_o = 1;
		e->sto[0].type = 'Q';
		return 1;
	} else
		return 0;
}

static int find_mask(const char *remain, struct expr *e)
{
	int i = 0;
	int res;

	while (isdigit(remain[i]) || remain[i] == '.')
		i++;
	if (i == 0)
		return 0;

	res = string2mask(remain, i - 1);
	if (res == BAD_MASK)
		return 0;
	return res;
}

static int find_expr(const char *remain, struct expr *e)
{
	int i = 0;
	int res;

	res = match_expr_single(e->end_expr, remain, e->sto, &i);
	e->num_o = i;
/*	e->skip_on_return = res; */
	return res;
}

static int find_char_range(const char *remain, struct expr *e)
{
	int res;

	res = match_char_against_range_clean(*remain, e->end_expr);
	return res;
}

/*
 * set_expression_canstop; called when '.*' expansion is need
 * will set e->canstop based on format string
 * parse_multiplier updates offset into 'in', 'fmt', the number of objects found (n_found)
 * @fmt      : the format string (the remain after '.*')
 * @e        : a pointer to a struct e to populate
 */
static int set_expression_canstop(const char *fmt, struct expr *e)
{
	int k = 1, res;

	e->num_o          = 0;
	e->skip_on_return = 0;
	e->can_skip       = 0;
	if (fmt[0] == '%') {
		/* find the conversion specifier after a field length */
		while (isdigit(fmt[k]))
			k++;
		e->end_expr_len = k + 1;
		switch (fmt[k]) {
		case '\0':
			debug(SCANF, 1, "Invalid format string '%s', ends with %%\n",
					fmt);
			return -1;
		case 'd':
			e->can_stop = &find_int;
			return 1;
		case 'u':
			e->can_stop = &find_uint;
			return 1;
		case 'x':
			e->can_stop = &find_hex;
			return 1;
		case 'l':
		case 'h':
			if (fmt[k + 1] == 'd')
				e->can_stop = &find_int;
			else if (fmt[k + 1] == 'u')
				e->can_stop = &find_uint;
			else if (fmt[k + 1] == 'x')
				e->can_stop = &find_hex;
			else {
				debug(SCANF, 1, "Invalid Conversion Specifier '%c%c'\n",
					fmt[k], fmt[k + 1]);
				return -1;
			}
			e->end_expr_len++;
			return 1;
		case 'I':
			e->can_stop = &find_ip;
			return 1;
		case 'Q':
			e->can_stop = &find_classfull_subnet;
			return 1;
		case 'P':
			e->can_stop = &find_subnet;
			return 1;
		case 'S':
			e->can_stop = &find_not_ip;
			return 1;
		case 'M':
			e->can_stop = &find_mask;
			return 1;
		case 'W':
			e->can_stop = &find_word;
			return 1;
		case 's':
			e->can_stop = &find_string;
			return 1;
		case '[':
			res = fill_char_range(e->end_expr, fmt + k,
					sizeof(e->end_expr));
			if (res < 0) {
				debug(SCANF, 1, "Invalid format '%s', unmatched '['\n",
						e->end_expr);
				return -1;
			}
			debug(SCANF, 4, "pattern matching will end on '%s'\n",
					e->end_expr);
			e->can_stop = &find_char_range;
			return 1;
		default:
			e->end_of_expr = fmt[0];
			e->can_stop = NULL;
			return 1;
		} /* switch c */
	} else if (fmt[0] == '(') {
		res = strxcpy_until(e->end_expr, fmt, sizeof(e->end_expr), ')');
		if (res < 0) {
			debug(SCANF, 1, "Invalid format '%s', unmatched '('\n", e->end_expr);
			return -1;
		}
		e->end_expr_len = res;
		debug(SCANF, 4, "pattern matching will end on '%s'\n", e->end_expr);
		e->can_stop = &find_expr;
	} else if (fmt[0] == '[') {
		res = fill_char_range(e->end_expr, fmt, sizeof(e->end_expr));
		if (res < 0) {
			debug(SCANF, 1, "Invalid format '%s', unmatched '['\n", e->end_expr);
			return -1;
		}
		debug(SCANF, 4, "pattern matching will end on '%s'\n", e->end_expr);
		e->can_stop = &find_char_range;
	} else if (fmt[0] == '\\') {
		e->end_of_expr = escape_char(fmt[1]);
		e->can_stop = NULL;
	} else {
		/* if not a special char, we try to find the first occurence of the next char
		 * after '.*' in FMT
		 */
		e->end_of_expr = fmt[0];
		e->can_stop = NULL;
	}
	return 1;
}

/*
 * parse_multiplier_xxx starts when fmt is a st_scanf multiplier char (*, +, ?, {a,b})
 * it will try to consume as many bytes as possible from 'in' and put objects
 * found in a struct sto *
 * parse_multiplier updates offset into 'in', 'fmt', the number of objects found (n_found)
 *
 * @in       : points to remaining input buffer
 * @fmt      : points to remaining fmt buffer
 * @in_max   : input buffer MUST be < in_max
 * @expr     : the string/expression  concerned by the multiplier
 * @o        : objects will be stored in o (max_o)
 * @n_found  : num conversion specifier found so far
 *
 * returns:
 *    positive on success
 *   -1  : format error
 *   -2  : no match
 */
static int parse_multiplier_char(const char **in, const char **fmt, const char *in_max,
		char *expr, struct sto *o, int max_o, int *n_found)
{
	int res;
	int min_m, max_m;
	int n_match = 0;

	if (**fmt == '{') {
		res = parse_brace_multiplier(*fmt, &min_m, &max_m);
		if (res < 0)
			return -1;
		*fmt += res;
	} else {
		min_m = min_match(**fmt);
		max_m = max_match(**fmt);
	}
	debug(SCANF, 5, "need to find expression '%s' {%d,%d} times\n", expr, min_m, max_m);
	/* simple case, we match a single char {n,m} times */
	debug(SCANF, 4, "Pattern expansion will end when in[j] != '%c'\n", *expr);
	while (n_match < max_m) {
		if (*expr != **in)
			break;
		*in += 1;
		n_match++;
		if (**in == '\0') {
			debug(SCANF, 3, "reached end of input scanning 'in'\n");
			break;
		}
	}
	if (n_match < min_m) {
		debug(SCANF, 3, "found char '%c' %d times, but required %d\n",
				*expr, n_match, min_m);
		return -2;
	}
	*fmt += 1;
	if (**in == '\0' && **fmt != '\0')
		return -2;
	return 1;
}

static int parse_multiplier_expr(const char **in, const char **fmt, const char *in_max,
		char *expr, struct sto *o, int max_o, int *n_found)
{
	int res, k, num_cs;
	int min_m, max_m;
	int n_match = 0;

	if (**fmt == '{') {
		res = parse_brace_multiplier(*fmt, &min_m, &max_m);
		if (res < 0)
			return -1;
		*fmt += res;
	} else {
		min_m = min_match(**fmt);
		max_m = max_match(**fmt);
	}
	num_cs = count_cs(expr);
	if (*n_found + num_cs > max_o) {
		debug(SCANF, 1, "Cannot get more than %d objets, already found %d\n",
				max_o, *n_found);
		return -1;
	}
	debug(SCANF, 4, "Pattern expansion will end when in[j] != '%s'\n", expr);
	while (n_match < max_m) {
		res = match_expr_single(expr, *in, o, n_found);
		if (res < 0) {
			debug(SCANF, 1, "Invalid format '%s'\n", expr);
			return -1;
		}
		if (res == 0)
			break;
		*in += res;
		n_match++;
		if (*in > in_max) {
			/* can happen only if there is a BUG in max_expr_single */
			fprintf(stderr, "BUG, input buffer override in %s line %d\n",
					__func__, __LINE__);
		}
		if (**in == '\0') {
			debug(SCANF, 3, "reached end of input scanning 'in'\n");
			break;
		}
	}
	if (n_match < min_m) {
		debug(SCANF, 3, "found char '%c' %d times, but required %d\n",
				*expr, n_match, min_m);
		return -2;
	}
	*fmt += 1;
	if (num_cs) {
		if (n_match) {
			debug(SCANF, 4, "found %d CS so far\n", *n_found);
		} else {
			/* 0 match but we found 'num_cs' conversion specifiers
			 * we must consume them because the caller add provisionned
			 * space for it
			 */
			debug(SCANF, 4, "0 match but there was %d CS so consume them\n",
					num_cs);
			for (k = 0; k < num_cs; k++) {
				o[*n_found].type = 0;
				*n_found += 1;
			}
		}
	}
	if (**in == '\0' && **fmt != '\0')
		return -2;
	return 1;
}

static int parse_multiplier_dotstar(const char **in, const char **fmt, const char *in_max,
		char *expr, struct sto *o, int max_o, int *n_found)
{
	int match_last, could_stop, previous_could_stop;
	int n_match;
	int min_m, max_m, k, res;
	struct expr e;
	const char *last_match_index = NULL;

	if (**fmt == '{') {
		res = parse_brace_multiplier(*fmt, &min_m, &max_m);
		if (res < 0)
			return -1;
		*fmt += res;
	} else {
		min_m = min_match(**fmt);
		max_m = max_match(**fmt);
	}
	debug(SCANF, 5, "need to find expression '.' {%d,%d} times\n", min_m, max_m);
	/*  '.*' handling ... BIG MESS */
	match_last = 0;
	n_match    = 0;
	could_stop = 0;
	previous_could_stop = 0;
	if (fmt[0][1] == '$') {
		if (max_m < 2) {
			debug(SCANF, 1, "'$' not allowed in this context, max expansion=%d\n",
					max_m);
			return -1;
		}
		match_last = 1;
		debug(SCANF, 4, "we will stop on the last match\n");
		*fmt += 1;
	}
	/* we need to find when the expr expansion will end */
	res = set_expression_canstop(*fmt + 1, &e);
	if (res < 0)
		return res;

	/* skipping min_m char, useless to match */
	*in     += min_m;
	n_match += min_m;
	/* handle case where min_m too big to match */
	if (*in > in_max)
		return -2;

	if (e.can_stop) {
		/* try to find at most max_m expr */
		while (n_match < max_m) {
			/* try to stop expansion */
			e.can_skip = 0;
			e.skip_on_return = 0;
			could_stop = e.can_stop(*in, &e);
			debug(SCANF, 4, "trying to stop on remaining '%s', res=%d\n",
					*in, could_stop);
			if (could_stop && match_last == 0)
				break;
			n_match++;
			/*
			 * last_match_index is set if current loop can stop AND previous cannot
			 *
			 * scanf("ab 121.1.1.1", ".*$I") should return '121.1.1.1' not '1.1.1.1'
			 * scanf("ab STRING", ".*$s")    should return 'STRING' not just 'G'
			 */
			if (could_stop && previous_could_stop == 0)
				last_match_index = *in;
			previous_could_stop = could_stop;
			*in += (e.can_skip ? e.can_skip : 1);

			if (*in > in_max) {
				/* can happen only if there is a BUG in can_skip usage */
				fprintf(stderr, "BUG, input buffer override in %s line %d\n",
						__func__, __LINE__);
				return -5;
			}
			if (**in == '\0') {
				debug(SCANF, 3, "reached end of input scanning 'in'\n");
				break;
			}
		}
	} else  {
		/* end on simple char */
		while (n_match < max_m) {
			/* try to stop expansion */
			could_stop = (**in == e.end_of_expr);
			debug(SCANF, 4, "trying to stop on char '%c', res=%d\n",
					e.end_of_expr, res);
			if (could_stop && match_last == 0)
				break;
			n_match++;
			if (could_stop && previous_could_stop == 0)
				last_match_index = *in;
			previous_could_stop = could_stop;
			*in += 1;
			if (**in == '\0') {
				debug(SCANF, 3, "reached end of input scanning 'in'\n");
				break;
			}
		}
	}
	debug(SCANF, 3, "Expr '.' matched %d times, could_stop=%d, skip=%d\n",
			n_match, could_stop, e.skip_on_return);
	/* in case of last match, we must rewind position in 'in'*/
	if (match_last) {
		*in = last_match_index;
		debug(SCANF, 4, "last match asked, rewind to previois pointer\n");
		if (*in == NULL) {
			debug(SCANF, 3, "last match never matched\n");
			return -2;
		}
	}
	*fmt += 1;
	/* if we stop a multiplier expansion on a complex conversion specifier, we may
	 * have recorded it in e->sto, to avoid anaylising it again
	 * ie scanf('.*%I a', '    1.1.1.1 a', must fully analyse 1.1.1.1 in '.*' expansion
	 * 1.1.1.1 was stored by 'find_ip' so use this instead of reanalysing again
	 */
	if (e.skip_on_return) {
		debug(SCANF, 3, "Trying to skip %d char already analysed\n", e.skip_on_return);
		for (k = 0; k < e.num_o; k++) {
			debug(SCANF, 3, "Restoring analysed object '%c'\n", e.sto[k].type);
			memcpy(&o[*n_found], &e.sto[k], sizeof(struct sto));
			*n_found += 1;
		}
		while (isdigit(**fmt)) /* remove field width */
			*fmt += 1;
		*fmt += 2;
		*in += e.skip_on_return;
	}
	/* multiplier went to the end of 'in', but without matching the end */
	if (**in == '\0' && **fmt != '\0')
		return -2;
	return 1;
}

/*
 * st_scanf CORE function
 * reads bytes from the buffer'in', tries to interpret/match againset regexp fmt
 * if objects (corresponding to covnersion specifiers) are found,
 * store them in struct sto_object *o table
 * @in    ; input  buffer
 * @fmt   : format buffer
 * @o     : will store input data (if conversion specifiers are found)
 * @max_o : max number of collected objects
 *
 * returns:
 *	number of objects found
 *	-1 if no match and no conversion specifier found
 */
int sto_sscanf(const char *in, const char *fmt, struct sto *o, int max_o)
{
	int res;
	int n_found;
	char c;
	char expr[128];
	int num_cs; /* number of conversion specifier found in an expression */
	int min_m = -1, max_m;
	const char *p, *f;
	const char *in_max; /* bound cheking of input pointer */

	p = in;  /* remaing in put buffer */
	f = fmt; /* remaing format buffer */
	n_found = 0; /* number of arguments/objects found */
	in_max = in + strlen(in);

	expr[0] = '\0';
	while (1) {
		debug(SCANF, 8, "Still to parse in FMT  : '%s'\n", f);
		debug(SCANF, 8, "Still to parse in 'in' : '%s'\n", p);
		if (is_multiple_char(*f)) {
			debug(SCANF, 1, "Invalid expr, 2 successives multipliers\n");
			return -1;
		}
		if (*f == '\0' && *p == '\0')
			return n_found;
		else if (*f == '\0')
			goto end_nomatch;
		else if (*p == '\0') { /* expr[i .... ] can match void, like '.*' */
			if (*f == '(' || *f == '[') {
				if (*f == '(')
					res = strxcpy_until(expr, f, sizeof(expr), ')');
				else
					res = fill_char_range(expr, f, sizeof(expr));
				if (res == -1) {
					debug(SCANF, 1, "Invalid format '%s', unmatched '%c'\n",
							fmt, c);
					goto end_nomatch;
				}
				f += res;
			} else {
				if (c == '\\')
					f++;
				f++;
			}
			if (is_multiple_char(*f)) {
				if (*f == '{') {
					res = parse_brace_multiplier(f, &min_m, &max_m);
					if (res < 0)
						goto end_nomatch;
					f += res;
				} else
					min_m = min_match(*f);
			}
			if (f[1] != '\0') /* the multiplier wasnt the last char */
				goto end_nomatch;
			if (min_m == 0) /* if the expr can match zero time, the match was perfect */
				return n_found;
			goto end_nomatch;
		} else if (*f == '%') {
			if (n_found > max_o - 1) {
				debug(SCANF, 1, "Max objets %d, already found %d\n",
						max_o, n_found);
				return n_found;
			}
			res = parse_conversion_specifier(&p, &f, o + n_found);
			if (res == 0)
				return n_found;
			n_found += res;
			/* any char */
		} else if (*f == '.') {
			f++;
			if (is_multiple_char(*f)) {
				res = parse_multiplier_dotstar(&p, &f, in_max, expr,
						o, max_o, &n_found);
				if (res < 0)
					goto end_nomatch;
				continue;
			}
			debug(SCANF, 8, "fmt='.', match any char\n");
			p++;
			/* expression or char range */
		} else if (*f == '(' || *f == '[') {
			if (*f == '(')
				res = strxcpy_until(expr, f, sizeof(expr), ')');
			else
				res = fill_char_range(expr, f, sizeof(expr));
			if (res == -1) {
				debug(SCANF, 1, "Invalid format '%s', unmatched '%c'\n", fmt, c);
				return n_found;
			}
			debug(SCANF, 8, "found expr '%s'\n", expr);
			f += res;
			if (is_multiple_char(*f)) {
				res = parse_multiplier_expr(&p, &f, in_max, expr,
						o, max_o, &n_found);
				if (res < 0)
					goto end_nomatch;
				continue;
			}
			num_cs = count_cs(expr);
			if (n_found + num_cs >= max_o) {
				debug(SCANF, 1, "Max objets %d, already found %d\n",
						max_o, n_found);
				return n_found;
			}
			res = match_expr_single(expr, p, o, &n_found);
			if (res < 0) {
				debug(SCANF, 1, "Invalid format '%s'\n", fmt);
				return n_found;
			}
			if (res == 0) {
				debug(SCANF, 2, "Expr '%s' didnt match 'in' at offset %d\n",
						expr, (int)(p - in));
				goto end_nomatch;
			}
			debug(SCANF, 4, "Expr '%s' matched 'in' res=%d at offset %d\n",
					expr, res, (int)(p - in));
			debug(SCANF, 4, "Found %d objects so far\n", n_found);
			p += res;
			if (p  > in_max) {
				/* can happen only if there is a BUG in 'match_expr_single'
				 * and its descendant
				 */
				fprintf(stderr, "BUG, input buffer override in %s line %d\n",
						__func__, __LINE__);
				return n_found;
			}
		} else {
			c = *f;
			if (*f == '\\') {
				f++;
				c = escape_char(*f);
				if (c == '\0') {
					debug(SCANF, 1, "Invalid format string '%s'\n", fmt);
					goto end_nomatch;
				}
			}
			if (is_multiple_char(f[1]))  {
				f++;
				expr[0] = c;
				res = parse_multiplier_char(&p, &f, in_max, expr,
						o, max_o, &n_found);
				if (res < 0)
					goto end_nomatch;
				continue;
			}
			if (*p != *f) {
				debug(SCANF, 2, "in[%d]='%c', != fmt[%d]='%c', exiting\n",
						 (int)(p - in), *p, (int)(fmt - f), *f);
				goto end_nomatch;
			}
			p++;
			f++;
		}
	} /* while 1 */
end_nomatch:
	if (n_found == 0)
		return -1;
	else
		return n_found;
}

int st_vscanf(const char *in, const char *fmt, va_list ap)
{
	int res;
	struct sto o[ST_VSCANF_MAX_OBJECTS];

	res = sto_sscanf(in, fmt, o, ST_VSCANF_MAX_OBJECTS);
	consume_valist_from_object(o, res, ap);
	return res;
}

int st_sscanf(const char *in, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = st_vscanf(in, fmt, ap);
	va_end(ap);
	return ret;
}
