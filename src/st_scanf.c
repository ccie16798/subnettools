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
#include "string2ip.h"
#include "debug.h"
#include "st_scanf.h"
#include "st_object.h"

#define ST_STRING_INFINITY 1000000000  /* Subnet tool definition of infinity */

/* st_scanf.c can be compiled for case insensitive pattern matching */
#ifdef CASE_INSENSITIVE
#define EVAL_CHAR(__c) tolower(__c)
#else
#define EVAL_CHAR(__c) (__c)
#endif

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
	case 't':
		return '\t';
	case 'n':
		return '\n';
	default:
		return EVAL_CHAR(c);
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
	debug(SCANF, 1, "BUG, Invalid quantifier char '%c'\n", c);
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
	debug(SCANF, 1, "BUG, Invalid quantifier char '%c'\n", c);
	return 0;
}

/*
 * parse a brace quantifier like {,1} or {2} or {3,4} {4,}
 * min & max are set by this helper
 * returns :
 *	-1 if string is invalid
 *	number of matched chars
 */
static int parse_brace_quantifier(const char *s, int *min, int *max)
{
	int i = 1;

	*min = 0;
	*max = ST_STRING_INFINITY;
	if (s[i] == '}') {
		debug(SCANF, 1, "Invalid empty quantifier '%s'\n", s);
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
 * opening '[' and closing ']' are removed
 * @expr  : the buffer to fill
 * @fmt   : the format string (fmt[0]='[' in this function)
 * @n     : max number of char to store in expr (including NUL)
 * returns:
 *     strlen(fmt) on SUCCESS
 *     -1 if fmt is badly formatted (no closing ']')
 */
static int fill_char_range(char *expr, const char *fmt, int n)
{
	int i = 0;

	/* to include a ']' in a range, it must be right after the opening ']' or after '^'
	 * we need to copy it before we enter the generic loop
	 */
	fmt++;
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
		expr[i] = EVAL_CHAR(fmt[i]);
		i++;
	}
	expr[i] = '\0';
	return i + 2; /* +2 since we remove opening [ and closing ] */
}

/*
 * from fmt string starting with '(', fill an expression
 * opening '(' and closing ')' are removed
 * @expr  : the buffer to fill
 * @fmt   : the format string (fmt[0]=='(' in this function)
 * @n     : max number of char to store in expr (including NUL)
 * returns:
 *     strlen(fmt) on SUCCESS
 *     -1 if fmt is badly formatted (no closing ']')
 */
static int fill_expr(char *expr, const char *fmt, int n)
{
	int i = 0, parenthese = 0;

	fmt++;
	if (*fmt == '|') /*and expression cannot start with an OR */
		return -1;
	while (1) {
		if (fmt[i] == '\\') { /* handle escape char */
			expr[i] = fmt[i];
			i++;
			if (fmt[i] == '\0' || i == n - 2)
				return -1;
			expr[i] = EVAL_CHAR(fmt[i]);
			i++;
			continue;
		}
		if (fmt[i] == '\0' || i == n - 2)
			return -1;
		if (fmt[i] == ')' && parenthese-- == 0)
			break;
		if (fmt[i] == '(')
			parenthese++;
		expr[i] = EVAL_CHAR(fmt[i]);
		i++;
	}
	expr[i] = '\0';
	return i + 2; /* +2 since we remove opening [ and closing ] */
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
	const char *p = *expr; /* cache expr to avoid dereferences */

	p++;
#ifdef CASE_INSENSITIVE
	c = EVAL_CHAR(c);
#endif
	if (*p == '^') {
		invert = 1;
		p++;
	}
	/* to include a ']' in a range, must be right after '[' of '[^' */
	if (*p == ']') {
		res = (c == ']');
		p++;
	}

	while (*p != ']') {
		low = EVAL_CHAR(*p);
		if (low == '\0') {
			debug(SCANF, 1, "Invalid expr '%s', no closing ']' found\n", *expr);
			return -1;
		}
		p++;
		if (*p == '-' && p[1] != ']') {
			p++;
			high = EVAL_CHAR(*p);
			if (high == '\0') {
				debug(SCANF, 1, "Invalid expr '%s', incomplete range\n", *expr);
				return -1;
			}
			if (c >= low && c <= high)
				res = 1;
			p++;
		} else {
			if (low == c)
				res = 1;
		}
	}
	p++;
	*expr = p;
	return invert ? !res : res;
}


/* match character c against STRING 'expr' like [acde-g]
 * We KNOW expr is a valid char range, no bound checking needed, opening [ and closing ]
 * have already been removed
 * @c    : the char to match
 * @expr : the expr to test c against
 * returns:
 *    1 if a match is found
 *    0 if no match
 */
static int match_char_against_range_clean(char c, const char *expr)
{
	char low;
	int direct = 1;

	if (*expr == '^') {
		direct = 0;
		expr++;
	}
#ifdef CASE_INSENSITIVE
	c = EVAL_CHAR(c);
#endif
	/* expr is garanteed to be 1 byte long or 2 bytes long if start with ^ */
	do {
		low = *expr;
		expr++;
		if (*expr == '-' && expr[1] != '\0') {
			expr++;
			if (c >= low && c <= *expr)
				return direct;
			expr++;
			continue;
		}
		if (low == c)
			return direct;
	} while (*expr != '\0');
	return !direct;
}

/* parse input STRING 'in' according to format 'fmt'
 * **fmt == '%' when the function starts (conversion specifier)
 * store output in o if not NULL, else put it to thrash
 * @in  : pointer to remaining input buffer
 * @fmt : pointer to remaining FORMAT buffer
 * @o   : a struct to store a found objet
 * returns:
 *	the number of conversion specifiers found (0 or 1)
*/
static int parse_conversion_specifier(const char **in, const char **fmt,
		struct sto *o)
{
	int n_found = 0; /* number of CS found */
	int i2, res;
	int max_field_length;
	char buffer[ST_OBJECT_MAX_STRING_LEN];
	char *ptr_buff;
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
	const char *f;

/* ARG_SET will set the storage for conversion specifier */
#define ARG_SET(__NAME, __TYPE) \
	do { \
		__NAME = (__TYPE)&o->s_char; \
		o->type = c; \
		o->conversion = 0; \
	} while (0)

	/* p is a pointer to 'in', f a pointer to 'fmt' */
	p = *in;
	f = (*fmt) + 1;
	/* computing field length */
	if (isdigit(*f)) {
		max_field_length = *f - '0';
		f++;
		while (isdigit(*f)) {
			max_field_length *= 10;
			max_field_length += *f - '0';
			f++;
		}
		if (max_field_length > sizeof(buffer) - 2)
			max_field_length = sizeof(buffer) - 2;
		debug(SCANF, 9, "Found max field length %d\n", max_field_length);
	} else
		max_field_length = sizeof(buffer) - 2;
	p_max = *in + max_field_length - 1; /* we reserve on char for NUL ending char */
	c = *f; /* c now points to the conversion specifier */
	switch (c) {
	case '\0':
		debug(SCANF, 1, "Invalid format '%s', ends with %%\n", *fmt);
		return n_found;
	case 'Q': /* classfull subnet */
	case 'P':
		ARG_SET(v_sub, struct subnet *);
		ptr_buff = buffer;
		while ((is_ip_char(*p) || *p == '/'))
			*ptr_buff++ = *p++; /* fast copy */
		*ptr_buff = '\0';
		if (p - *in <= 2) {
			debug(SCANF, 3, "no IP found at %s\n", *in);
			return n_found;
		}
		if (c == 'P')
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
		f += 1;
		c = *f;
		if (c != 'd' && c != 'u' && c != 'x') {
			debug(SCANF, 1, "Invalid format '%s', wrong char '%c' after 'h'\n",
					*fmt, c);
			return n_found;
		}
		ARG_SET(v_short, short *);
		o->conversion = 'h';
		if (c == 'x') {
			if (*p == '0' && p[1] == 'x')
				p += 2;
			if (!isxdigit(*p)) {
				debug(SCANF, 3, "no HEX found at %s\n", *in);
				return n_found;
			}
			*v_short = char2int(*p);
			p++;
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
			*v_short = (*p - '0');
			p++;
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
		f += 1;
		c = *f;
		if (c != 'd' && c != 'u' && c != 'x') {
			debug(SCANF, 1, "Invalid format, wrong char '%c' after 'l'\n", c);
			return n_found;
		}
		ARG_SET(v_long, long *);
		o->conversion = 'l';
		if (c == 'x') {
			if (*p == '0' && p[1] == 'x')
				p += 2;
			if (!isxdigit(*p)) {
				debug(SCANF, 3, "no HEX found at %s\n", *in);
				return n_found;
			}
			*v_long = char2int(*p);
			p++;
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
			*v_long = char2int(*p);
			p++;
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
		if (*p == '-') {
			sign = -1;
			p++;
		} else
			sign = 1;
		if (!isdigit(*p)) {
			debug(SCANF, 3, "no INT found at %s\n", *in);
			return n_found;
		}
		*v_int = (*p - '0');
		p++;
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
		if (!isdigit(*p)) {
			debug(SCANF, 3, "no UINT found at %s\n", *in);
			return n_found;
		}
		*v_uint = (*p - '0');
		p++;
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
		if (*p == '0' && p[1] == 'x')
			p += 2;
		if (!isxdigit(*p)) {
			debug(SCANF, 3, "no HEX found at %s\n", *in);
			return n_found;
		}
		*v_uint = char2int(*p);
		p++;
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
		if (f[1] == '.') {
			debug(SCANF, 1, "Invalid format, found '.' after %%s\n");
			return n_found;
		} else if (f[1] == '%') {
			debug(SCANF, 1, "Invalid format, found '%%' after %%s\n");
			return n_found;
		}
		ptr_buff = v_s;
		while (!isspace(*p) && *p != '\0' && p < p_max)
			*ptr_buff++ = *p++; /* fast copy */

		if (p == *in) {
			debug(SCANF, 3, "no STRING found at %s\n", *in);
			return n_found;
		}
		*ptr_buff = '\0';
		if (c == 'S') {
			res = get_subnet_or_ip(v_s, (struct subnet *)&buffer);
			if (res > 0) {
				debug(SCANF, 3, "STRING '%s' is an IP\n", v_s);
				return n_found;
			}
		}
		debug(SCANF, 5, "found STRING len='%d' value '%s'\n",
				(int)(ptr_buff - v_s), v_s);
		n_found++;
		break;
	case 'W':
		ARG_SET(v_s, char *);
		ptr_buff = v_s;
		while (isalpha(*p) && p < p_max)
			*ptr_buff++ = *p++;

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
		i2 = fill_char_range(expr, f, sizeof(expr));
		if (i2 == -1) {
			debug(SCANF, 1, "Invalid format '%s', no closing ']'\n", *fmt);
			return n_found;
		}
		f += (i2 - 1);
		i2 = 0;
		ptr_buff = v_s;
		while (match_char_against_range_clean(*p, expr) && *p != '\0' && p < p_max)
			*ptr_buff++ = *p++;

		if (p == *in) {
			debug(SCANF, 3, "no CHAR RANGE '%s' found at %s\n", expr, *in);
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
	*fmt = f + 1;
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
		c = EVAL_CHAR(*expr);
		debug(SCANF, 8, "remaining in  ='%s'\n", in);
		debug(SCANF, 8, "remaining expr='%s'\n", expr);
		switch (c) {
		case '\0':
			return in - saved_in;
		case '|':
			return in - saved_in;
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
			res = parse_conversion_specifier(&in, &expr, o + *num_o);
			if (res == 0)
				break;
			debug(SCANF, 4, "conv specifier successfull '%c' for %d\n",
						o[*num_o].type, *num_o);
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
			if (EVAL_CHAR(*in) != c)
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
	if (i > 9) {
		debug(SCANF, 1, "Cannot have more than %d specifiers in an expression\n",
			10);
		i = 9;
	}
	e->num_o = i;
	/* we will not analyse the expression again on return */
	e->skip_on_return = res;
	e->can_skip = res;
	return res;
}

static int find_char_range(const char *remain, struct expr *e)
{
	int res;

	res = match_char_against_range_clean(*remain, e->end_expr);
	return res;
}

/*
 * set_expression_canstop; called when '.*' expansion is needed
 * will set e->can_stop handler based on format string
 * @fmt      : the format string (the remain after '.*')
 * @e        : a pointer to a struct expr to populate
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
			e->end_of_expr = EVAL_CHAR(fmt[0]);
			e->can_stop = NULL;
			return 1;
		} /* switch c */
	} else if (fmt[0] == '(') {
		res = fill_expr(e->end_expr, fmt, sizeof(e->end_expr));
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
		/* if not a special char, we try to find the first occurence of the next char
		 * after '.*' in FMT;
		 * Note that NUL char is a perfectly valid char in this case
		 */
		e->end_of_expr = escape_char(fmt[1]);
		e->can_stop = NULL;
	} else {
		e->end_of_expr = EVAL_CHAR(fmt[0]);
		e->can_stop = NULL;
	}
	return 1;
}

/*
 * parse_quantifier_xxx starts when **fmt is a st_scanf quantifier char (*, +, ?, {a,b})
 * it will try to consume as many bytes as possible from 'in' and put objects
 * found in a struct sto *
 * parse_quantifier_xxx updates offset into 'in', 'fmt', the number of objects found (n_found)
 *
 * @in       : points to remaining input buffer
 * @fmt      : points to remaining fmt buffer
 * @in_max   : input buffer MUST be < in_max
 * @expr     : the string/expression  concerned by the quantifier
 * @o        : objects will be stored in o (max_o)
 * @n_found  : num conversion specifier found so far
 *
 * returns:
 *    positive on success
 *   -1  : format error
 *   -2  : no match
 */
static int parse_quantifier_char(const char **in, const char **fmt, const char *in_max,
		char *expr, struct sto *o, int max_o, int *n_found)
{
	int res;
	int min_m, max_m;
	int n_match = 0;
	const char *p = *in; /* p caches '*in' to avoid dereferences and speed up */

	if (**fmt == '{') {
		res = parse_brace_quantifier(*fmt, &min_m, &max_m);
		if (res < 0)
			return -1;
		*fmt += res + 1;
	} else {
		min_m = min_match(**fmt);
		max_m = max_match(**fmt);
		*fmt += 1;
	}
	debug(SCANF, 5, "need to find expression '%s' {%d,%d} times\n", expr, min_m, max_m);
	/* simple case, we match a single char {n,m} times */
	debug(SCANF, 4, "Pattern expansion will end when in[j] != '%c'\n", *expr);
	while (n_match < max_m) {
		if (*expr != EVAL_CHAR(*p))
			break;
		p += 1;
		n_match++;
		if (*p == '\0') {
			debug(SCANF, 3, "reached end of input scanning 'in'\n");
			break;
		}
	}
	if (n_match < min_m) {
		debug(SCANF, 3, "found char '%c' %d times, but required %d\n",
				*expr, n_match, min_m);
		return -2;
	}
	if (*p == '\0' && **fmt != '\0')
		return -2;
	*in = p;
	return 1;
}

static int parse_quantifier_char_range(const char **in, const char **fmt, const char *in_max,
		char *expr, struct sto *o, int max_o, int *n_found)
{
	int res;
	int min_m, max_m;
	int n_match = 0;
	const char *p = *in; /* p caches '*in' to avoid dereferences and speed up */

	if (**fmt == '{') {
		res = parse_brace_quantifier(*fmt, &min_m, &max_m);
		if (res < 0)
			return -1;
		*fmt += res + 1;
	} else {
		min_m = min_match(**fmt);
		max_m = max_match(**fmt);
		*fmt += 1;
	}
	debug(SCANF, 4, "Pattern expansion will end when in[j] != '%s'\n", expr);
	while (n_match < max_m) {
		res = match_char_against_range_clean(*p, expr);
		if (res == 0)
			break;
		p += 1;
		n_match++;
		if (*p == '\0') {
			debug(SCANF, 3, "reached end of input scanning 'in'\n");
			break;
		}
	}
	if (n_match < min_m) {
		debug(SCANF, 3, "found char '%c' %d times, but required %d\n",
				*expr, n_match, min_m);
		return -2;
	}
	if (*p == '\0' && **fmt != '\0')
		return -2;
	*in = p;
	return 1;
}

static int parse_quantifier_expr(const char **in, const char **fmt, const char *in_max,
		char *expr, struct sto *o, int max_o, int *n_found)
{
	int res, k, num_cs;
	int min_m, max_m;
	int n_match = 0;
	const char *p = *in; /* p caches '*in' to avoid dereferences and speed up */

	if (**fmt == '{') {
		res = parse_brace_quantifier(*fmt, &min_m, &max_m);
		if (res < 0)
			return -1;
		*fmt += res + 1;
	} else {
		min_m = min_match(**fmt);
		max_m = max_match(**fmt);
		*fmt += 1;
	}
	num_cs = count_cs(expr);
	if (*n_found + num_cs > max_o) {
		debug(SCANF, 1, "Cannot get more than %d objets, already found %d\n",
				max_o, *n_found);
		return -1;
	}
	debug(SCANF, 4, "Pattern expansion will end when in[j] != '%s'\n", expr);
	while (n_match < max_m) {
		res = match_expr_single(expr, p, o, n_found);
		if (res < 0) {
			debug(SCANF, 1, "Invalid format '%s'\n", expr);
			return -1;
		}
		if (res == 0)
			break;
		p += res;
		n_match++;
		if (p > in_max) {
			/* can happen only if there is a BUG in max_expr_single */
			fprintf(stderr, "BUG, input buffer override in %s line %d\n",
					__func__, __LINE__);
			return -5;
		}
		if (*p == '\0') {
			debug(SCANF, 3, "reached end of input scanning 'in'\n");
			break;
		}
	}
	if (n_match < min_m) {
		debug(SCANF, 3, "found char '%c' %d times, but required %d\n",
				*expr, n_match, min_m);
		return -2;
	}
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
	if (*p == '\0' && **fmt != '\0')
		return -2;
	*in = p;
	return 1;
}

static int parse_quantifier_dotstar(const char **in, const char **fmt, const char *in_max,
		char *expr, struct sto *o, int max_o, int *n_found)
{
	int match_last, could_stop, previous_could_stop;
	int last_skip_on_return, last_end_expr_len, last_num_o;
	int n_match;
	int min_m, max_m, k, res;
	struct expr e;
	const char *last_match_index = NULL;
	const char *p = *in;

	if (**fmt == '{') {
		res = parse_brace_quantifier(*fmt, &min_m, &max_m);
		if (res < 0)
			return -1;
		*fmt += res + 1;
	} else {
		min_m = min_match(**fmt);
		max_m = max_match(**fmt);
		*fmt += 1;
	}
	debug(SCANF, 5, "need to find expression '.' {%d,%d} times\n", min_m, max_m);
	/*  '.*' handling ... BIG MESS */
	match_last = 0;
	n_match    = 0;
	previous_could_stop = 0;
	if (**fmt == '$') {
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
	res = set_expression_canstop(*fmt, &e);
	if (res < 0)
		return res;

	/* skipping min_m char, useless to match */
	p       += min_m;
	n_match += min_m;
	/* handle case where min_m too big to match */
	if (p > in_max)
		return -2;

	/* handle end on complex expression (Conversion specifier, expression ...) **/
	if (e.can_stop) {
		/* try to find at most max_m expr */
		while (n_match < max_m) {
			/* try to stop expansion */
			e.can_skip = 0;
			e.skip_on_return = 0;
			could_stop = e.can_stop(p, &e);
			debug(SCANF, 4, "trying to stop on remaining '%s', res=%d\n",
					p, could_stop);
			if (could_stop && match_last == 0)
				break;
			n_match++;
			/*
			 * last_match_index is set if current loop can stop AND previous cannot
			 *
			 * scanf("ab 121.1.1.1", ".*$I") should return '121.1.1.1' not '1.1.1.1'
			 * scanf("ab STRING", ".*$s")    should return 'STRING' not just 'G'
			 */
			if (could_stop && previous_could_stop == 0) {
				/* we must save information to restore on last_match */
				last_match_index    = p;
				last_skip_on_return = e.skip_on_return;
				last_end_expr_len   = e.end_expr_len;
				last_num_o          = e.num_o;
			}
			previous_could_stop = could_stop;
			p += (e.can_skip ? e.can_skip : 1);

			if (p > in_max) {
				/* can happen only if there is a BUG in can_skip usage */
				fprintf(stderr, "BUG, input buffer override in %s line %d\n",
						__func__, __LINE__);
				return -5;
			}
			if (*p == '\0') {
				debug(SCANF, 3, "reached end of input scanning 'in'\n");
				break;
			}
		}
	} else  {
		/* handle end on simple char */
		while (n_match < max_m) {
			/* try to stop expansion */
			could_stop = (EVAL_CHAR(*p) == e.end_of_expr);
			debug(SCANF, 4, "trying to stop on char '%c', res=%d\n",
					e.end_of_expr, res);
			if (could_stop && match_last == 0)
				break;
			n_match++;
			if (could_stop && previous_could_stop == 0) {
				last_match_index    = p;
				last_skip_on_return = e.skip_on_return;
				last_end_expr_len   = e.end_expr_len;
				last_num_o          = e.num_o;
			}
			previous_could_stop = could_stop;
			p += 1;
			if (*p == '\0') {
				debug(SCANF, 3, "reached end of input scanning 'in'\n");
				break;
			}
		}
	}
	debug(SCANF, 3, "Expr '.' matched %d times, could_stop=%d, skip=%d\n",
			n_match, could_stop, e.skip_on_return);
	/* in case of last match, we must rewind position in 'in'*/
	if (match_last) {
		p                = last_match_index;
		e.skip_on_return = last_skip_on_return;
		e.end_expr_len   = last_end_expr_len;
		e.num_o          = last_num_o;
		/* we dont need to restore e.sto, because expr hasnt matched any more */
		debug(SCANF, 4, "last match asked, rewind to previous pointer\n");
		if (p == NULL) {
			debug(SCANF, 3, "last match never matched\n");
			return -2;
		}
	}
	/* if we stop a quantifier expansion on a complex conversion specifier, we may
	 * have recorded it in e->sto, to avoid anaylising it again
	 * ie scanf('.*%I a', '    1.1.1.1 a', must fully analyse 1.1.1.1 in '.*' expansion
	 * 1.1.1.1 was stored by 'find_ip' so use this instead of re-analysing again
	 */
	if (e.skip_on_return) {
		debug(SCANF, 3, "Trying to skip %d input chars, %d fmt chars\n",
				 e.skip_on_return, e.end_expr_len);
		for (k = 0; k < e.num_o; k++) {
			debug(SCANF, 3, "Restoring analysed object '%c'\n", e.sto[k].type);
			memcpy(&o[*n_found], &e.sto[k], sizeof(struct sto));
			*n_found += 1;
		}
		*fmt += e.end_expr_len;
		p    += e.skip_on_return;
	}
	/* quantifier went to the end of 'in', but without matching the end */
	if (*p == '\0' && **fmt != '\0')
		return -2;
	*in = p;
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
#ifdef CASE_INSENSITIVE
int sto_sscanf_ci(const char *in, const char *fmt, struct sto *o, int max_o)
#else
int sto_sscanf(const char *in, const char *fmt, struct sto *o, int max_o)
#endif
{
	int res;
	int n_found;
	char c;
	char expr[128];
	int num_cs; /* number of conversion specifier found in an expression */
	int min_m = -1, max_m;
	const char *p, *f;
	const char *in_max; /* bound checking of input pointer */

	p = in;  /* remaining input  buffer */
	f = fmt; /* remaining format buffer */
	n_found = 0; /* number of arguments/objects found */
	in_max = in + strlen(in);

	while (1) {
		debug(SCANF, 8, "Still to parse in FMT  : '%s'\n", f);
		debug(SCANF, 8, "Still to parse in 'in' : '%s'\n", p);
		if (*p == '\0') { /* remaining format string may match, like '.*' */
			if (*f == '\0') /* perfect match */
				return n_found;
			if (*f == '(')
				res = fill_expr(expr, f, sizeof(expr));
			else if (*f == '[')
				res = fill_char_range(expr, f, sizeof(expr));
			else if (*f == '\\')
				res = 2;
			else
				res = 1;
			if (res < 0) /* wrong expression/char range */
				goto end_nomatch;
			f += res;
			if (is_multiple_char(*f)) {
				if (*f == '{') {
					res = parse_brace_quantifier(f, &min_m, &max_m);
					if (res < 0)
						goto end_nomatch;
					f += res;
				} else
					min_m = min_match(*f);
			}
			if (f[1] != '\0') /* the quantifier wasnt the last char */
				goto end_nomatch;
			if (min_m == 0) /* if the expr can match zero time, the match was perfect */
				return n_found;
			goto end_nomatch;
		}

		switch (*f) {
		case '\0': /* if we are here 'in' wasnt fully consumed, so fail */
			goto end_nomatch;
		case '*': /* if we found a quantifier char here that means */
		case '+': /* we have two consecutive quantifier chars or */
		case '?': /* fmt starts with a quantifier */
		case '{':
			if (f == fmt)
				debug(SCANF, 1, "Invalid expr, starts with a quantifier\n");
			else
				debug(SCANF, 1, "Invalid expr, 2 successives quantifiers\n");
			goto end_nomatch;
		case '%': /* conversion specifier */
			if (n_found > max_o - 1) {
				debug(SCANF, 1, "Max objets %d, already found %d\n",
						max_o, n_found);
				return n_found;
			}
			res = parse_conversion_specifier(&p, &f, o + n_found);
			if (res == 0)
				return n_found;
			n_found += res;
			continue;
		case '.': /* any char */
			f++;
			if (is_multiple_char(*f)) {
				res = parse_quantifier_dotstar(&p, &f, in_max, expr,
						o, max_o, &n_found);
				if (res < 0)
					goto end_nomatch;
				continue;
			}
			p++;
			continue;
			/* expression or char range */
		case '[':
			res = fill_char_range(expr, f, sizeof(expr));
			if (res == -1) {
				debug(SCANF, 1, "Invalid format '%s', unmatched '%c'\n", fmt, *f);
				return n_found;
			}
			debug(SCANF, 8, "found char range '%s'\n", expr);
			f += res;
			if (is_multiple_char(*f)) {
				res = parse_quantifier_char_range(&p, &f, in_max, expr,
						o, max_o, &n_found);
				if (res < 0)
					goto end_nomatch;
				continue;
			}
			res = match_char_against_range_clean(*p, expr);
			if (res == 0) {
				debug(SCANF, 2, "Range '%s' didnt match 'in' at offset %d\n",
						expr, (int)(p - in));
				goto end_nomatch;
			}
			debug(SCANF, 4, "Range '%s' matched 'in' offset %d\n",
					expr, (int)(p - in));
			p++;
			continue;
		case '(':
			res = fill_expr(expr, f, sizeof(expr));
			if (res == -1) {
				debug(SCANF, 1, "Invalid format '%s', unmatched '%c'\n", fmt, *f);
				return n_found;
			}
			debug(SCANF, 8, "found expr '%s'\n", expr);
			f += res;
			if (is_multiple_char(*f)) {
				res = parse_quantifier_expr(&p, &f, in_max, expr,
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
			continue;
		default:
			c = *f;
			if (c == '\\') {
				f++;
				c = escape_char(*f);
				if (c == '\0') {
					debug(SCANF, 1, "Invalid format string '%s'\n", fmt);
					goto end_nomatch;
				}
			}
			if (is_multiple_char(f[1]))  {
				f++;
				expr[0] = EVAL_CHAR(c);
				res = parse_quantifier_char(&p, &f, in_max, expr,
						o, max_o, &n_found);
				if (res < 0)
					goto end_nomatch;
				continue;
			}
			if (EVAL_CHAR(*p) != EVAL_CHAR(c)) {
				debug(SCANF, 2, "in[%d]='%c', != fmt[%d]='%c', exiting\n",
						 (int)(p - in), *p, (int)(fmt - f), *f);
				goto end_nomatch;
			}
			p++;
			f++;
		} /* switch */
	} /* while 1 */
end_nomatch:
	if (n_found == 0)
		return -1;
	else
		return n_found;
}

#ifdef CASE_INSENSITIVE
int st_vscanf_ci(const char *in, const char *fmt, va_list ap)
{
	int res;
	struct sto o[ST_VSCANF_MAX_OBJECTS];

	res = sto_sscanf_ci(in, fmt, o, ST_VSCANF_MAX_OBJECTS);
	consume_valist_from_object(o, res, ap);
	return res;
}

int st_sscanf_ci(const char *in, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = st_vscanf_ci(in, fmt, ap);
	va_end(ap);
	return ret;
}
#else

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
#endif
