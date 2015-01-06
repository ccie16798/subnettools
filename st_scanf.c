/*
 * IPv4, IPv6 subnet/routes scanf equivalent with PATTERN matching
 *
 * Copyright (C) 2014,2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
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

// TO check : bound checking
// (Expr1|expr2) handling
// {a,b} multiplier
// READ HEX :x; :hx

struct expr {
	char *expr;
	int (*stop)(char *remain, struct expr *e);
	char end_of_expr; /* if remain[i] = end_of_expr , we can stop*/
	char end_expr[64];
	int match_last; /* if set, the expansion will stop the last time ->stop return positive value && and previous stop DIDNOT*/
	int last_match; /* the last pos in input string where the ->stop(remain, e) matched and ->stop(remain - 1, e) DIDNOT */
	int last_nmatch;
	int has_stopped;
	int skip_stop; /* if positive, dont run e->stop */
};


/* return the escaped char */
static inline char escape_char(char input_c) {
	char c;

	c = input_c;
	switch (c) {
		case '\0':
			debug(SCANF, 2, "Nul char after Escape Char '\\' \n");
			break;
		case 't':
			c = '\t';
			break;
		case 'n':
			c = '\n';
			break;
		default:
			break;
	}
	return c;
}

static int is_multiple_char(char c) {
	return (c == '*' || c == '+' || c == '?' || c == '{');
}

/*
 * find the conversion specifier char after a '%'
 * (since it can the followed by a max_field_length)
 */
static char conversion_specifier(const char *fmt) {
	int i = 0;

	while (1) {
		if (fmt[i] == '\0')
			return '\0';
		if (!isdigit(fmt[i]))
			return fmt[i];
		i++;
	}
	return '\0';
}

static int max_match(char c) {
	if (c == '*') return 1000000000;
	if (c == '+') return 1000000000;
	if (c == '?') return 1;
	debug(SCANF, 1, "BUG, Invalid multiplier char '%c'\n", c);
	return 0;
}

static int min_match(char c) {
	if (c == '*') return 0;
	if (c == '+') return 1;
	if (c == '?') return 0;
	debug(SCANF, 1, "BUG, Invalid multiplier char '%c'\n", c);
	return 0;
}

/*
 * parse a brace multiplier like {,1} or {2} or {3,4} {4,}
 * min & max are set by this helper
 * return -1 if string is invalid, number of matched char
 */
static int parse_brace_multiplier(const char *s, int *min, int *max) {
	int i = 1;

	*min = 0;
	*max = 10000000;
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
			return i + 1;
		} else if (isdigit(s[i])) {
			*max = 0;
			while (isdigit(s[i])) {
				*max *= 10;
				*max += s[i] - '0';
				i++;
			}
			if (s[i] != '}') {
				debug(SCANF, 1, "Invalid multiplier '%s', no closing '}'\n", s);
				return -1;
			}
			if (min > max) {
				debug(SCANF, 1, "Invalid range, min:%d > max:%d\n", *min, *max);
				return -1;
			}
		}
	} else if (s[i] == '}') {
		*max = *min;
		return i + 1;
	} else {
		debug(SCANF, 1, "Invalid range '%s' contains invalid char '%c'\n", s, s[i]);
		return -1;
	}
	return -1;
}
/* count number of consersion specifier in an expr
 * doesnt validate CS are valid
 */
static int count_cs(const char *expr) {
	int i, n;
	n = 0;
	if (expr[0] == '%')
		n++;
	for (i = 1; ;i++) {
		if (expr[i] == '\0')
			return n;
		if (expr[i] == '%' && expr[i - 1] != '\\') /* unlike regular scanf, escape char is '\' and only that */
			n++;
	}
	return 0;
}

/* given an expression e, the following function
 * try to determine if the remaining chars match their type
 * for exemple, find_int will match on the first digit found
*/
static int find_int(char *remain, struct expr *e) {
	return isdigit(*remain) || (*remain == '-' && isdigit(remain[1]));
}

static int find_uint(char *remain, struct expr *e) {
	return isdigit(*remain);
}

static int find_word(char *remain, struct expr *e) {
	return isalpha(*remain);
}

static int find_string(char *remain, struct expr *e) {
	return !isspace(*remain);
}

static int find_hex(char *remain, struct expr *e) {
	if (remain[0] == '0' && remain[1] == 'x' && isxdigit(remain[2]))
		return 1;
	return isxdigit(*remain);
}


/*
 * from fmt string starting with '[', fill expr with the range
 * a bit like strxpy_until, but not quite since a ']' is allowed right after the opeing '[' or a '^' if present
 */
static int fill_char_range(char *expr, const char *fmt, int n) {
	int i = 1;

	/* to include a ']' in a range, it must be right after the opening ']' or after '^'
	 * we need to copy it before we enter the generic loop  */
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
		if (fmt[i] == '\0' || i == n - 1)
			return -1;
		expr[i] = fmt[i];
		i++;
	}
	expr[i] = ']';
	expr[i + 1] = '\0';
	return i + 1;
}
/*
 * match character c against STRING expr like [acde-g]
 * *i is a pointer to curr index in expr
 * returns :
 *    1 if a match is found
 *    0 if no match
 *    -1 if range is invalid
 */
static int match_char_against_range(char c, const char *expr, int *i) {
	int res = 0;
	char low, high;
	int invert = 0;

	*i += 1;
	if (expr[*i] == '^') {
		invert = 1;
		*i += 1;
	}
	/* to include a ']' in a range, must be right after '[' of '[^' */
	if (expr[*i] == ']') {
		res = (c == ']');
		*i += 1;
	}

	while (expr[*i] != ']') {
		low = expr[*i];
		if (low == '\0') {
				debug(SCANF, 1, "Invalid expr '%s', no closing ']' found\n", expr);
				return -1;
		}
		/* expr[*i + 2] != ']' means we can match a '-' only if it is right before the ending ']' */
		if (expr[*i + 1] == '-' && expr[*i + 2] != ']' ) {
			high = expr[*i + 2];
			if (high == '\0') {
				debug(SCANF, 1, "Invalid expr '%s', incomplete range\n", expr);
				return -1;
			}
			if (c >= low && c <= high)
				res = 1;
		} else {
			if (low == c)
				res = 1;
		}
		*i += 1;
	}
	*i += 1;
	if (invert)
		return !res;
	else
		return res;
}
/* parse STRING in at index *j according to fmt at index *i
   store output in o if not NULL, else put it to thrash
   fmt = FORMAT buffer
   in  = input buffer
   i   = index in fmt
   j   = index in in
*/
static int parse_conversion_specifier(char *in, const char *fmt,
		int *i, int *j, struct sto *o) {
	int n_found = 0;
	int i2, j2, res;
	int max_field_length;
	char buffer[128];
	char poubelle[256];
	char c;
	struct subnet *v_sub;
	char expr[64];
	struct ip_addr *v_addr;
	char *v_s;
	long *v_long;
	int *v_int;
	short *v_short;
	unsigned int *v_uint;
	int sign;

#define ARG_SET(__NAME, __TYPE) do { \
	if (o == NULL) \
	__NAME = (__TYPE)&poubelle;  \
	else { \
	__NAME = (__TYPE)&o->s_char; \
	o->type = fmt[*i + 1]; \
	o->conversion = 0; \
	} \
} while (0);

	j2 = *j;
	/* computing field length */
	if (isdigit(fmt[*i + 1])) {
		max_field_length = fmt[*i + 1] - '0';
		*i += 2;
		while (isdigit(fmt[*i])) {
			max_field_length *= 10;
			max_field_length += fmt[*i] - '0';
			*i += 1;
		}
		*i -= 1;
		if (max_field_length > sizeof(buffer) - 2)
			max_field_length = sizeof(buffer) - 2;
		debug(SCANF, 4, "Found max field length %d\n", max_field_length);
	} else
		max_field_length = sizeof(buffer) - 2;

	switch (fmt[*i + 1]) {
		case '\0':
			debug(SCANF, 1, "Invalid format '%s', ends with %%\n", fmt);
			return n_found;
		case 'Q': /* classfull subnet */
		case 'P':
			ARG_SET(v_sub, struct subnet *);
			while ((is_ip_char(in[j2])||in[j2] == '/')) {
				buffer[j2 - *j] = in[j2];
				j2++;
			}
			buffer[j2 - *j] = '\0';
			if (j2 - *j <= 2) {
				debug(SCANF, 2, "no IP found at offset %d\n", *j);
				return n_found;
			}
			if (fmt[*i + 1] == 'P')
				res = get_subnet_or_ip(buffer, v_sub);
			else
				res = classfull_get_subnet(buffer, v_sub);
			if (res < 1000) {
				debug(SCANF, 5, "'%s' is a valid IP\n", buffer);
				n_found++;
			} else {
				debug(SCANF, 2, "'%s' is an invalid IP\n", buffer);
				return n_found;
			}
			break;
		case 'I':
			ARG_SET(v_addr, struct ip_addr *);
			while (is_ip_char(in[j2])) {
				buffer[j2 - *j] = in[j2];
				j2++;
			}
			buffer[j2 - *j] = '\0';
			if (j2 - *j <= 1) {
				debug(SCANF, 2, "no IP found at offset %d\n", *j);
				return n_found;
			}
			res = string2addr(in + *j, v_addr, j2 -*j);
			if (res < 1000) {
				debug(SCANF, 5, "'%s' is a valid IP\n", buffer);
				n_found++;
			} else {
				debug(SCANF, 2, "'%s' is an invalid IP\n", buffer);
				return n_found;
			}
			break;
		case 'M':
			ARG_SET(v_int, int *);
			while ((isdigit(in[j2]) || in[j2] == '.')) {
				buffer[j2 - *j] = in[j2];
				j2++;
			}
			buffer[j2 - *j] = '\0';
			if (j2 - *j == 0) {
				debug(SCANF, 2, "no MASK found at offset %d\n", *j);
				return n_found;
			}
			res = string2mask(buffer, 21);
			if (res != BAD_MASK) {
				debug(SCANF, 5, "'%s' is a valid MASK\n", buffer);
				n_found++;
			} else {
				debug(SCANF, 2, "'%s' is an invalid MASK\n", buffer);
				return n_found;
			}
			*v_int = res;
			break;
		case 'h':
			*i += 1;
			if (fmt[*i + 1] != 'd' && fmt[*i + 1] != 'u' && fmt[*i + 1] != 'x') {
				debug(SCANF, 1, "Invalid format '%s', only specifier allowed after %%h are 'd', 'u', 'x'\n", fmt);
				return n_found;
			}
			ARG_SET(v_short, short *);
			if (o)
				o->conversion = 'h';
			*v_short = 0;
			if (fmt[*i + 1] == 'x') {
				if (in[j2] == '0' && in[j2 + 1] == 'x')
					j2 += 2;
				if (!isxdigit(in[j2])) {
					debug(SCANF, 2, "no HEX found at offset %d \n", *j);
					return n_found;
				}
				while (isxdigit(in[j2])) {
					*v_short <<= 4;
					*v_short += char2int(in[j2]);
					j2++;
				}
				debug(SCANF, 5, "found short HEX '%x' at offset %d\n", *v_short, *j);
			} else {
				if (in[*j] == '-' && fmt[*i + 1] == 'd') {
					sign = -1;
					j2++;
				} else
					sign = 1;
				if (!isdigit(in[j2])) {
					return n_found;
				}
				while (isdigit(in[j2]) && j2 - *j < max_field_length) {
					debug(SCANF, 2, "no SHORT found at offset %d \n", *j);
					*v_short *= 10;
					*v_short += (in[j2] - '0') ;
					j2++;
				}
				*v_short *= sign;
				if (fmt[*i + 1] == 'u') {
					debug(SCANF, 5, "found USHORT '%hu' at offset %d\n", (unsigned short)*v_short, *j);
				} else {
					debug(SCANF, 5, "found SHORT '%hd' at offset %d\n", *v_short, *j);
				}
			}
			n_found++;
			break;
		case 'l':
			*i += 1;
			if (fmt[*i + 1] != 'd' && fmt[*i + 1] != 'u' && fmt[*i + 1] != 'x') {
				debug(SCANF, 1, "Invalid format '%s', only specifier allowed after %%l are 'd', 'u', 'x'\n", fmt);
				return n_found;
			}
			ARG_SET(v_long, long *);
			if (o)
				o->conversion = 'l';
			*v_long = 0;
			if (fmt[*i + 1] == 'x') {
				if (in[j2] == '0' && in[j2 + 1] == 'x')
					j2 += 2;
				if (!isxdigit(in[j2])) {
					debug(SCANF, 2, "no HEX found at offset %d \n", *j);
					return n_found;
				}
				while (isxdigit(in[j2])) {
					*v_long <<= 4;
					*v_long += char2int(in[j2]);
					j2++;
				}
				debug(SCANF, 5, "found long HEX '%lx' at offset %d\n", (unsigned long)*v_long, *j);
			} else {
				if (in[*j] == '-' && fmt[*i + 1] == 'd') {
					sign = -1;
					j2++;
				} else
					sign = 1;
				if (!isdigit(in[j2])) {
					debug(SCANF, 2, "no LONG found at offset %d \n", *j);
					return n_found;
				}
				while (isdigit(in[j2]) && j2 - *j < max_field_length) {
					*v_long *= 10UL;
					*v_long += (in[j2] - '0') ;
					j2++;
				}
				*v_long *= sign;
				if (fmt[*i + 1] == 'u') {
					debug(SCANF, 5, "found ULONG '%lu' at offset %d\n", (unsigned long)*v_long, *j);
				} else {
					debug(SCANF, 5, "found LONG '%ld' at offset %d\n", *v_long, *j);
				}
			}
			n_found++;
			break;
		case 'd':
			ARG_SET(v_int, int *);
			*v_int = 0;
			if (in[*j] == '-') {
				sign = -1;
				j2++;
			} else
				sign = 1;
			if (!isdigit(in[j2])) {
				debug(SCANF, 2, "no INT found at offset %d \n", *j);
				return n_found;
			}
			while (isdigit(in[j2])) {
				*v_int *= 10;
				*v_int += (in[j2] - '0') ;
				j2++;
			}
			*v_int *= sign;
			debug(SCANF, 5, "found INT '%d' at offset %d\n", *v_int, *j);
			n_found++;
			break;
		case 'u':
			ARG_SET(v_uint, unsigned int *);
			*v_uint = 0;
			if (!isdigit(in[j2])) {
				debug(SCANF, 2, "no UINT found at offset %d \n", *j);
				return n_found;
			}
			while (isdigit(in[j2])) {
				*v_uint *= 10;
				*v_uint += (in[j2] - '0') ;
				j2++;
			}
			debug(SCANF, 5, "found UINT '%u' at offset %d\n", *v_uint, *j);
			n_found++;
			break;
		case 'x':
			ARG_SET(v_uint, unsigned int *);
			*v_uint = 0;
			if (in[j2] == '0' && in[j2 + 1] == 'x')
				j2 += 2;
			if (!isxdigit(in[j2])) {
				debug(SCANF, 2, "no HEX found at offset %d \n", *j);
				return n_found;
			}
			while (isxdigit(in[j2])) {
				*v_uint <<= 4;
				*v_uint += char2int(in[j2]);
				j2++;
			}
			debug(SCANF, 5, "found HEX '%x' at offset %d\n", *v_uint, *j);
			n_found++;
			break;
		case 'S': /* a special STRING that doesnt represent an IP */
		case 's':
			ARG_SET(v_s, char *);
			c = fmt[*i + 2];
			if (c == '.') {
				debug(SCANF, 1, "Invalid format '%s', found '.' after %%s\n", fmt);
				return n_found;
			} else if (c == '%') {
				debug(SCANF, 1, "Invalid format '%s', found '%%' after %%s\n", fmt);
				return n_found;
			}
			while (! isspace(in[j2]) && j2 - *j < max_field_length - 1) {
				if (in[j2] == '\0')
					break;
				v_s[j2 - *j] = in[j2];
				j2++;
			}
			if (j2 == *j) {
				debug(SCANF, 2, "no STRING found at offset %d \n", *j);
				return n_found;
			}
			v_s[j2 - *j] = '\0';
			if (fmt[*i + 1] == 'S') {
				res = get_subnet_or_ip(v_s, (struct subnet *)&poubelle);
				if (res < 1000) {
					debug(SCANF, 2, "STRING '%s' at offset %d is an IP, refusing it\n", v_s, *j);
					return n_found;
				}

			}
			debug(SCANF, 5, "found STRING '%s' starting at offset %d \n", v_s, *j);
			n_found++;
			break;
		case 'W':
			ARG_SET(v_s, char *);
			while (isalpha(in[j2]) && j2 - *j < max_field_length - 1) {
				v_s[j2 - *j] = in[j2];
				j2++;
			}
			if (j2 == *j) {
				debug(SCANF, 2, "no WORD found at offset %d\n", *j);
				return 0;
			}
			v_s[j2 - *j] = '\0';
			debug(SCANF, 5, "WORD '%s' found at offset %d\n", v_s,  *j);
			n_found++;
			break;
		case '[':
			ARG_SET(v_s, char *);
			i2 = fill_char_range(expr, fmt + *i + 1, sizeof(expr));
			if (i2 == -1) {
				debug(SCANF, 1, "Invalid format '%s', no closing ']'\n", fmt);
				return n_found;
			}
			*i += (i2 - 1);
			i2 = 0;
			/* match_char_against_range cant return -1 here, fill_char_range above would have caught a bad expr */
			while (match_char_against_range(in[j2], expr, &i2) && j2 - *j < max_field_length - 1) {
				if (in[j2] == '\0')
					break;
				v_s[j2 - *j] = in[j2];
				i2 = 0;
				j2++;
			}
			if (j2 == *j) {
				debug(SCANF, 2, "no CHAR RANGE found at offset %d\n", *j);
				return 0;
			}
			v_s[j2 - *j] = '\0';
			debug(SCANF, 5, "CHAR RANGE '%s' found at offset %d\n", v_s,  *j);
			n_found++;
			break;
		case 'c':
			ARG_SET(v_s, char *);
			v_s[0] = in[*j];
			debug(SCANF, 5, "CHAR '%c' found at offset %d\n", *v_s,  *j);
			j2 = *j + 1;
			n_found++;
		default:
			debug(SCANF, 1, "Unknow conversion specifier '%c'\n", fmt[*i + 1]);
			break;
	} /* switch */
	*j = j2;
	*i += 2;
	return n_found;
}

/*
 * try to find a '|' char in the remaining expr
 * return -1 if none found, the next expression if one is found
 * if expr = AB | CD | EF, expr_try_again will return '4' (index of the space char after '|')
 */
static inline int expr_try_again(const char *expr) {
	int i;

	for (i = 0; ; i++) {
		if (expr[i] == '\0')
			return -1;
		if (expr[i] == '|')
			return i + 1;
	}
	return -1;
}

/*
 * match a single pattern 'expr' against 'in'
 * returns :
 *  0 if it doesnt match,
 *  number of matched chars in input buffer if it matches
 *
 * if 'expr' includes conversion specifiers, put the result in 'o' and update 'num_o'
 */
static int match_expr_single(const char *expr, char *in, struct sto *o, int *num_o) {
	int i, j, j2, res;
	int a = 0;
	char c;
	int saved_num_o = *num_o;

	i = 0; /* index in expr */
	j = 0; /* index in input buffer */

	while (1) {
		c = expr[i];
		if (c == '\0' || c =='|')
			return a;
		debug(SCANF, 8, "remaining in  ='%s'\n", in + j);
		debug(SCANF, 8, "remaining expr='%s'\n", expr + i);
		switch (c) {
			case '(':
				i++;
				break;
			case ')':
				i++;
				break;
			case '.':
				i++;
				j++;
				a++;
				break;
			case '[': /* try to handle char range like [a-Zbce-f] */
				res = match_char_against_range(in[j], expr, &i);
				if (res <= 0)
					goto try_again;
				a++;
				j++;
				break;
			case '%':
				debug(SCANF, 3, "conversion specifier to handle %lu\n", (unsigned long)(o + *num_o));
				j2 = j;
				res = parse_conversion_specifier(in, expr, &i, &j, &o[*num_o]);
				if (res == 0)
					goto try_again;
				if (o) {
					debug(SCANF, 4, "conv specifier successfull '%c' for %d\n", o[*num_o].type, *num_o);
				} else {
					debug(SCANF, 4, "conv specifier successfull\n");
				}
				*num_o += 1;
				a += (j - j2);
				break;
			case '\\':
				i++;
				c = escape_char(expr[i]);
				if (c == '\0') {
					debug(SCANF, 1, "expr '%s' invalid, '\\' at end of string\n", expr);
					return 0;
				}
			default:
				if (c == '|')
					return a;
				if (in[j] != c)
					goto try_again;
				i++;
				j++;
				a++;
				break;
		}
		continue;
		try_again:
			res = expr_try_again(expr + i);
			if (res == -1) /* no '|' */
				return 0;
			i += res;
			j = 0;
			a = 0;
			*num_o = saved_num_o;
			debug(SCANF, 4, "We have been given another chance, remaing expr '%s'\n", expr + i);
	}
}
/* match expression e against input buffer
 * will return :
 * 0 if it doesnt match
 * n otherwise
 */
static int match_expr(struct expr *e, char *in, struct sto *o, int *num_o) {
	int res = 0;
	int res2;
	int saved_num_o = *num_o;

	res = match_expr_single(e->expr, in, o, num_o);
	debug(SCANF, 4, "Matching expr '%s' against in '%s' res=%d numo='%d'\n", e->expr, in, res, *num_o);
	if (res < 0)
		return res;
	if (res == 0) {
		*num_o = saved_num_o;
		return 0;
	}
	/* e->skip_stop positive means we wont test the remain to see if we need to stop */
	if (e->skip_stop > 0) {
		e->skip_stop -= res;
		return res;
	}
	/* even if 'in' matches 'e', we may have to stop */
	e->has_stopped = 0;
	if (e->stop) {
		res2 = e->stop(in, e);
		debug(SCANF, 4, "trying to stop on '%s', res=%d\n", in, res2);
	} else {
		res2 = (*in == e->end_of_expr);
		debug(SCANF, 4, "trying to stop on '%c', res=%d\n", e->end_of_expr, res2);
	}
	if (res2) {
		/* if we matched a CS but decide we need to stop, we must restore
		  *num_o to its  original value */
		*num_o = saved_num_o;
		if (e->match_last) { /* if we need find the last match, lie about the result */
			e->has_stopped = 1;
			return res;
		}
		return 0;
	}
	return res;
}

static int find_not_ip(char *remain, struct expr *e) {
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
	if (get_subnet_or_ip(buffer, &s)  < 1000) {
		e->skip_stop = i; /* remain[0...i] represents an IP, so dont try stop checking in that range */
		return 0;
	} else
		return 1;
}

static int find_ip(char *remain, struct expr *e) {
	char buffer[64];
	int i = 0;
	struct subnet s;

	while (is_ip_char(remain[i]) && i < sizeof(buffer)) {
		buffer[i] = remain[i];
		i++;
	}
	if (i <= 1)
		return 0;
	buffer[i] = '\0';
	if (get_subnet_or_ip(buffer, &s)  < 1000) {
		e->skip_stop = i; /* useful for .$* matching */
		return 1;
	}
	else
		return 0;
}

static int find_classfull_subnet(char *remain, struct expr *e) {
	char buffer[64];
	int i = 0;
	struct subnet s;

	while ((is_ip_char(remain[i]) || remain[i] == '/') && i < sizeof(buffer)) {
		buffer[i] = remain[i];
		i++;
	}
	if (i <= 2)
		return 0;
	buffer[i] = '\0';
	if (classfull_get_subnet(buffer, &s)  < 1000) {
		e->skip_stop = i; /* useful for .$* matching */
		return 1;
	}
	else
		return 0;
}

static int find_mask(char *remain, struct expr *e) {
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

static int find_expr(char *remain, struct expr *e) {
	int i = 0;
	int res;

	res = match_expr_single(e->end_expr, remain, NULL, &i);
	return res;
}

int sto_sscanf(char *in, const char *fmt, struct sto *o, int max_o) {
	int i, j, k;
	int res;
	int min_m, max_m;
	int n_found, n_match;
	char c;
	char expr[64];
	struct expr e;
	int in_length;
	int e_has_stopped;
	int num_cs; /* number of conversion specifier found in an expression */

	i = 0; /* index in fmt */
	j = 0; /* index in in */
	n_found = 0; /* number of arguments/objects found */
	in_length = (int)strlen(in);
	expr[0] = '\0';
	while (1) {
		c = fmt[i];
		debug(SCANF, 8, "Still to parse in FMT  : '%s'\n", fmt + i);
		debug(SCANF, 8, "Still to parse in 'in' : '%s'\n", in + j);
		if (is_multiple_char(c)) {
			if (c == '{') {
				res = parse_brace_multiplier(fmt + i, &min_m, &max_m);
				if (res < 0)
					return n_found;
			} else {
				min_m = min_match(c);
				max_m = max_match(c);
			}
			e.expr = expr;
			e.end_of_expr = fmt[i + 1]; /* if necessary */
			e.stop = NULL;
			e.last_match  = -1;
			e.last_nmatch = -1;
			e.match_last  = 0;
			e.skip_stop   = 0;
			num_cs = count_cs(expr);
			if (n_found + num_cs > max_o) {
				debug(SCANF, 1, "Cannot get more than %d objets, already found %d\n", max_o, n_found);
				return n_found;
			}
			debug(SCANF, 5, "need to find expression '%s' %c time, with %d conversion specifiers\n", expr, c, num_cs);
			if (fmt[i + 1] == '$') {
				if (max_m < 2) {
					debug(SCANF, 1, "'$' special char is only allowed after mutiple expansion chars like '*', '+'\n");

				} else {
					e.match_last = 1;
					debug(SCANF, 4, "we will stop on the last match\n");
				}
				i++;
			}
			/* we need to find when the expr expansion will end, in case it matches anything like '.*' */
			if (fmt[i + 1] == '%') {
				c = conversion_specifier(fmt + i + 2);

				switch (c) {
					case '\0':
						debug(SCANF, 1, "Invalid format string '%s', ends with %%\n", fmt);
						return n_found;
					case 'd':
						e.stop = &find_int;
						break;
					case 'u':
						e.stop = &find_uint;
						break;
					case 'x':
						e.stop = find_hex;
						break;
					case 'l':
					case 'h':
						if (fmt[i + 3] == 'd')
							e.stop = &find_int;
						else if (fmt[i + 3] == 'u')
							e.stop = &find_uint;
						else if (fmt[i + 3] == 'x')
							e.stop = &find_hex;
						break;
					case 'I':
						e.stop = &find_ip;
						break;
					case 'Q':
						e.stop = &find_classfull_subnet;
						break;
					case 'P':
						e.stop = &find_ip;
						break;
					case 'S':
						e.stop = &find_not_ip;
						break;
					case 'M':
						e.stop = &find_mask;
						break;
					case 'W':
						e.stop = &find_word;
						break;
					case 's':
						e.stop = &find_string;
						break;
					case '[':
						k = 0;
						while (isdigit(fmt[i + k + 2])) /* we must take field length into account */
							k++;
						res = fill_char_range(e.end_expr, fmt + i + k + 2, sizeof(e.end_expr));
						if (res < 0) {
							debug(SCANF, 1, "Bad format '%s', unmatched '['\n", expr);
							return n_found;
						}
						debug(SCANF, 4, "pattern matching will end on '%s'\n", e.end_expr);
						e.stop = &find_expr;
						break;
					default:
						break;
				} /* switch c */
			} else if (fmt[i + 1] == '(') {
				res = strxcpy_until(e.end_expr, fmt + i + 1, sizeof(e.end_expr), ')');
				if (res < 0) {
					debug(SCANF, 1, "Bad format '%s', unmatched '('\n", expr);
					return n_found;
				}
				debug(SCANF, 4, "pattern matching will end on '%s'\n", e.end_expr);
				e.stop = &find_expr;
			} else if (fmt[i + 1] == '[') {
				res = fill_char_range(e.end_expr, fmt + i + 1, sizeof(e.end_expr));
				if (res < 0) {
					debug(SCANF, 1, "Bad format '%s', unmatched '['\n", expr);
					return n_found;
				}
				debug(SCANF, 4, "pattern matching will end on '%s'\n", e.end_expr);
				e.stop = &find_expr;
			}
			n_match = 0; /* number of time expression 'e' matches input*/
			/* try to find at most max_m expr */
			e_has_stopped = 0;
			while (n_match < max_m) {
				res = match_expr(&e, in + j, o, &n_found);
				if (res < 0) {
					debug(SCANF, 1, "Bad format '%s'\n", expr);
					return n_found;
				}
				if (res == 0)
					break;
				/* we check also if the previous invocation return positive or not
				   scanf("abdsdfdsf t e 121.1.1.1", ".*$I") should return '121.1.1.1' not '1.1.1.1'
				   scanf("abdsdfdsf t e STRING", ".*$s") should return 'STRING' not just 'G'
				 */
				if (e.has_stopped && e_has_stopped == 0) {
					e.last_match = j;
					e.last_nmatch = n_match + 1;
				}
				e_has_stopped = e.has_stopped;
				j += res;
				n_match++;
				if (in[j] == '\0') {
					debug(SCANF, 3, "reached end of input scanning 'in'\n");
					break;
				}
				if (j > in_length) { /* can happen only if there is a BUG in 'match_expr' and its descendant */
					fprintf(stderr, "BUG, input buffer override in %s line %d\n", __FUNCTION__, __LINE__);
					return n_found;
				}
			}
			debug(SCANF, 3, "Exiting loop with expr '%s' matched %d times, found %d objects so far\n", expr, n_match, n_found);
			if (e.match_last) {
				j       = e.last_match;
				n_match = e.last_nmatch;
				debug(SCANF, 3, "but last match asked so lets rewind to '%d' matches\n",  n_match);
			}

			if (n_match < min_m) {
				debug(SCANF, 1, "found expr '%s' %d times, but required %d\n", expr, n_match, min_m);
				return n_found;
			}
			if (num_cs) {
				if (n_match > 1) {
					// FIXME
					//debug(SCANF, 1, "conversion specifier matching in a pattern is supported only with '?'; restoring only %d found objects\n", *num_o);
				}
				if (n_match) {
					debug(SCANF, 4, "found %d CS so far\n", n_found);
				} else {
					/* 0 match but we had num_cs conversion specifier
					   we must consume them */
					debug(SCANF, 4, "0 match but there was %d CS so consume them\n", num_cs);
					for (k = 0; k < num_cs; k++) {
						o[n_found].type = 0;
						n_found += 1;
					}
				}
			}
			i++;
			continue;
		}
		if (c == '\0' || in[j] == '\0') {
			return n_found;
		} else if (c == '%') {
			if (n_found >= max_o - 1) {
				debug(SCANF, 1, "Cannot get more than %d objets, already found %d\n", max_o, n_found);
				return n_found;
			}
			res = parse_conversion_specifier(in, fmt, &i, &j, o + n_found);

			if (res == 0)
				return n_found;
			n_found += res;
		} else if (c == '.') {
			if (is_multiple_char(fmt[i + 1])) {
				expr[0] = c;
				expr[1] = '\0';
				i++;
				continue;
			}
			debug(SCANF, 8, "fmt[%d]='.', match any char\n", i);
			i++;
			j++;
		} else if (c == '(' || c == '[') {
			char c2 = (c == '(' ? ')' : ']');
			if (c == '(') {
				c2 = ')';
				res = strxcpy_until(expr, fmt + i, sizeof(expr), c2);
			} else {
				c = ']';
				res = fill_char_range(expr, fmt + i, sizeof(expr));
			}
			if (res == -1) {
				debug(SCANF, 1, "Invalid format '%s', unmatched '%c'\n", fmt, c2);
				return n_found;
			}
			debug(SCANF, 8, "found expr '%s'\n", expr);
			i += res;
			if (is_multiple_char(fmt[i]))
				continue;

			num_cs = count_cs(expr);
			if (n_found + num_cs >= max_o) {
				debug(SCANF, 1, "Cannot get more than %d objets, already found %d\n", max_o, n_found);
				return n_found;
			}
			res = match_expr_single(expr, in + j, o, &n_found);
			if (res < 0) {
				debug(SCANF, 1, "Bad format '%s'\n", fmt);
				return n_found;
			}
			if (res == 0) {
				debug(SCANF, 2, "Expr'%s' didnt match in 'in' at offset %d\n", expr, j);
				return n_found;
			}
			debug(SCANF, 4, "Expr'%s' matched 'in' res=%d at offset %d, found %d objects so far\n", expr, res, j, n_found);
			j += res;
			if (j > in_length) { /* can happen only if there is a BUG in 'match_expr_single' and its descendant */
				fprintf(stderr, "BUG, input buffer override in %s line %d\n", __FUNCTION__, __LINE__);
				return n_found;
			}
		} else {
			if (fmt[i] == '\\') {
				c = escape_char(fmt[i + 1]);
				if (c == '\0') {
					debug(SCANF, 1, "Invalid format string '%s', nul char after escape char\n", fmt);
					return n_found;
				}
				i++;
				if (is_multiple_char(fmt[i + 1])) {
					expr[0] = '\\';
					expr[1] = c;
					expr[2] = '\0';
					i++;
					continue;
				}
			}
			if (is_multiple_char(fmt[i + 1]))  {
				expr[0] = c;
				expr[1] = '\0';
				i++;
				continue;
			}
			if (in[j] != fmt[i]) {
				debug(SCANF, 2, "in[%d]='%c', != fmt[%d]='%c', exiting\n",
						j, in[j], i, fmt[i]);
				return  n_found;
			}
			i++;
			j++;
		}
	} /* while 1 */
}

int st_vscanf(char *in, const char *fmt, va_list ap) {
	int res;
	struct sto o[40];

	res = sto_sscanf(in, fmt, o, 40);
	consume_valist_from_object(o, res, ap);
	return res;
}

int st_sscanf(char *in, const char *fmt, ...) {
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = st_vscanf(in, fmt, ap);
	va_end(ap);
	return ret;
}
