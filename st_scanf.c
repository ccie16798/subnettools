#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "utils.h"
#include "iptools.h"
#include "debug.h"
#include "st_scanf.h"

// TO FIX : bound checking
// specifier at EOS
// %[ handling

struct expr {
	int num_expr;
	char *expr[10];
	int (*stop)(char *remain, struct expr *e);
	char end_of_expr; /* if remain[i] = end_of_expr , we can stop*/
	char end_expr[64];
	int match_last;
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
	return (c == '*' || c == '+' || c == '?');
}

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

/* count number of consersion specifier in an expr
 * doesnt validate CS are valid
 */
static int count_cs(const char *expr) {
	int i, n;
	n = 0;
	for (i = 0; ;i++) {
		if (expr[i] == '\0')
			return n;
		if (expr[i] == '%')
			n++;
	}
	return 0;
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

/* given an expression e, the following function
 * try to determine if the remaining chars match their type
 * for exemple, find_int will match on the first digit found
*/
static int find_int(char *remain, struct expr *e) {
	return isdigit(*remain) || (*remain == '-' && isdigit(remain[1]));
}

static int find_word(char *remain, struct expr *e) {
	return isalpha(*remain);
}

static int find_string(char *remain, struct expr *e) {
	return !isspace(*remain);
}


/*
 * from fmt string starting with '[', fill expr with the range
 */
int fill_char_range(const char *fmt, char *expr) {
	int i = 0;

	while (fmt[i] != ']') {
		if (fmt[i] == '\0')
			return -1;
		expr[i] = fmt[i];
		i++;
	}
	expr[i] = ']';
	expr[i + 1] = '\0';
	return i;
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
	while (expr[*i] != ']') {
		low = expr[*i];
		if (low == '\0') {
				debug(SCANF, 1, "Invalid expr '%s', no closing ']' found\n", expr);
				return -1;
		}
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
   store output in o if not NULL, else consumes ap
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
	int sign;

#define ARG_SET(__NAME, __TYPE) do { \
	if (o == NULL) \
	__NAME = (__TYPE)&poubelle;  \
	else { \
	__NAME = (__TYPE)&o->s_char; \
	o->type = fmt[*i + 1]; \
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
			debug(SCANF, 1, "Invalid format string '%s', ends with %%\n", fmt);
			return n_found;
		case 'P':
			ARG_SET(v_sub, struct subnet *);
			while ((is_ip_char(in[j2])||in[j2] == '/') && j2 - *j < max_field_length) {
				buffer[j2 - *j] = in[j2];
				j2++;
			}
			buffer[j2 - *j] = '\0';
			if (j2 - *j <= 2) {
				debug(SCANF, 2, "no IP found at offset %d\n", *j);
				return n_found;
			}
			res = get_subnet_or_ip(buffer, v_sub);
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
			while (is_ip_char(in[j2]) && j2 - *j < max_field_length) {
				buffer[j2 - *j] = in[j2];
				j2++;
			}
			buffer[j2 - *j] = '\0';
			if (j2 - *j <= 2) {
				debug(SCANF, 2, "no IP found at offset %d\n", *j);
				return n_found;
			}
			res = get_single_ip(in + *j, v_addr, j2 -*j);
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
			while ((isdigit(in[j2]) || in[j2] == '.') && j2 - *j < max_field_length) {
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
		case 'l':
			*i += 1;
			if (fmt[*i + 1] != 'd') {
				debug(SCANF, 1, "Invalid format '%s', only specifier allowed after %%l is 'd'\n", fmt);
				/* but we treat it as long int */
			}
			ARG_SET(v_long, long *);
			*v_long = 0;
			if (in[*j] == '-') {
				sign = -1;
				j2++;
			} else
				sign = 1;
			if (!isdigit(in[j2])) {
				debug(SCANF, 2, "no LONG found at offset %d \n", *j);
				return n_found;
			}
			while (isdigit(in[j2]) && j2 - *j < max_field_length) {
				*v_long *= 10;
				*v_long += (in[j2] - '0') ;
				j2++;
			}
			*v_long *= sign;
			debug(SCANF, 5, "found LONG '%ld' at offset %d\n", *v_long, *j);
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
			while (isdigit(in[j2]) && j2 - *j < max_field_length) {
				*v_int *= 10;
				*v_int += (in[j2] - '0') ;
				j2++;
			}
			*v_int *= sign;
			debug(SCANF, 5, "found INT '%d' at offset %d\n", *v_int, *j);
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
			while (in[j2] != ' ' && j2 - *j < max_field_length - 1) {
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
			i2 = fill_char_range(fmt + *i + 1, expr);
			if (i2 == -1) {
					debug(SCANF, 1, "Invalid format '%s', no closing ]\n", fmt);
					return n_found;
			}
			*i += i2;
			i2 = 0;
			while (match_char_against_range(in[j2], expr, &i2)) {
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

#define consume_valist_from_object(__o, __n, __ap) do { \
	int __i; \
	void *ptr; \
	\
	for (__i = 0; __i < __n; __i++) { \
		debug(SCANF, 8, "restoring '%c' to va_list\n", __o[__i].type); \
		ptr = va_arg(__ap, void *); \
		switch (__o[__i].type) { \
			case '[': \
			case 's': \
			case 'W': \
			case 'S': \
				  strcpy((char *)ptr, __o[__i].s_char); \
			break; \
			case 'I': \
				  copy_ipaddr((struct ip_addr *)ptr, &__o[__i].s_addr); \
			break; \
			case 'P': \
				  copy_subnet((struct subnet *)ptr, &__o[__i].s_subnet); \
			break; \
			case 'd': \
			case 'M': \
				  *((int *)ptr) = __o[__i].s_int; \
			break; \
			case 'l': \
				  *((long *)ptr) = __o[__i].s_long; \
			break; \
			case 'c': \
				  *((char *)ptr) = __o[__i].s_char[0]; \
			break; \
			default: \
				 debug(SCANF, 1, "Unknown object type '%c'\n", __o[__i].type); \
		} \
	} \
	\
} while (0);

/*
 * match a single pattern 'expr' against 'in'
 * returns 0 if doesnt match, number of matched char in input buffer
 * if expr has a conversion specifier, put the result in 'o' 
 */
static int match_expr_single(const char *expr, char *in, struct sto *o, int *num_o) {
	int i, j, res;
	int a = 0;
	char c;
	int saved_num_o = *num_o;

	i = 0; /* index in expr */
	j = 0; /* index in input buffer */

	while (1) {
		c = expr[i];
		if (c == '\0')
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
				if (res == -1)
					return -1;
				if (res)
					a++;
				j++;
				break;
			case '%':
				debug(SCANF, 3, "conversion specifier to handle %lu\n", (unsigned long)(o + *num_o));
				res = parse_conversion_specifier(in, expr, &i, &j, o + *num_o);
				if (res == 0)
					return a;
				if (o) {
					debug(SCANF, 4, "conv specifier successfull '%c'\n", o[*num_o].type);
				} else {
					debug(SCANF, 4, "conv specifier successfull\n");
				}
				*num_o += 1;
				a += j;
				break;
			case '\\':
				i++;
				c = escape_char(expr[i]);
			default:
				if (in[j] != c) {
					*num_o = saved_num_o;
					return 0;
				}
				if (c == '\0')
					return a;
				i++;
				j++;
				a++;
				break;
		}

	}
}
/* match expression e against input buffer
 * will return :
 * 0 if it doesnt match
 * n otherwise
 */
static int match_expr(struct expr *e, char *in, struct sto *o, int *num_o) {
	int i = 0;
	int res = 0;
	int res2;
	int saved_num_o = *num_o;

	for (i = 0; i < e->num_expr; i++) {
		res = match_expr_single(e->expr[i], in, o + *num_o, num_o);
		debug(SCANF, 4, "Matching expr '%s' against input '%s' res=%d\n", e->expr[i], in, res);
		if (res)
			break;
	}
	if (res == 0)
		return 0;
	/* even if 'in' matches 'e', we may have to stop */
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
		return 0;
	}
	return res;
}

static int find_ip(char *remain, struct expr *e) {
	char buffer[64];
	int i = 0;
	struct subnet s;

	while (is_ip_char(remain[i]) && i < sizeof(buffer)) {
		buffer[i] = remain[i];
		i++;
	}
	if (i <= 2)
		return 0;
	buffer[i] = '\0';
	if (get_subnet_or_ip(buffer, &s)  < 1000)
		return 1;
	else
		return 0;
}

static int find_charrange(char *remain, struct expr *e) {
	int i = 0;
	int res;

	res = match_expr_single(e->end_expr, remain, NULL, &i);
	return res;
}

static int st_vscanf(char *in, const char *fmt, va_list ap) {
	int i, j, i2, k;
	int res;
	int min_m, max_m;
	int n_found, n_match;
	char c;
	char expr[64];
	struct expr e;
	struct sto sto[20];
	struct sto *o;
	int num_o; /* number of objects found inside an expression */
	int num_cs; /* number of conversion specifier found in an expression */

	o = sto;
	i = 0; /* index in fmt */
	j = 0; /* index in in */
	n_found = 0; /* number of arguments found */
	expr[0] = '\0';
	while (1) {
		c = fmt[i];
		debug(SCANF, 8, "Still to parse in FMT  : '%s'\n", fmt + i);
		debug(SCANF, 8, "Still to parse in 'in' : '%s'\n", in + j);
		if (is_multiple_char(c)) {
			min_m = min_match(c);
			max_m = max_match(c);
			e.expr[0] = expr;
			e.num_expr = 1;
			e.end_of_expr = fmt[i + 1]; /* if necessary */
			e.stop = NULL;
			e.match_last = 0;
			num_cs = count_cs(expr);
			debug(SCANF, 5, "need to find expression '%s' %c time, with %d conversion specifiers\n", expr, c, num_cs);
			if (fmt[i + 1] == '%') {
				c = conversion_specifier(fmt + i + 2);
				if (c == '\0') {
					debug(SCANF, 1, "Invalid format string '%s', ends with %%\n", fmt);
					return n_found;
				} else if (c == 'd') {
					e.stop = &find_int;
				} else if (c == 'I') {
					e.stop = &find_ip;
				} else if (c == 'P') {
					e.stop = &find_ip;
				} else if (c == 'M') {
					e.stop = &find_int;
				} else if (c == 'W') {
					e.stop = &find_word;
				} else if (c == 's') {
					e.stop = &find_string;
				} else if (c == 'c') {
				} else if (c == '[') {
					res = strxcpy_until(e.end_expr, fmt + i + 2, sizeof(e.end_expr), ']');
					if (res < 0) {
						debug(SCANF, 1, "Unmatched '[', closing\n");
						return n_found;
					}
					debug(SCANF, 4, "pattern matching will end on '%s'\n", e.end_expr);
					e.stop = &find_charrange;
				}
			} else if (fmt[i + 1] == '(') {
				res = strxcpy_until(e.end_expr, fmt + i + 1, sizeof(e.end_expr), ')');
				if (res < 0) {
					debug(SCANF, 1, "Unmatched '(', closing\n");
					return n_found;
				}
				debug(SCANF, 4, "pattern matching will end on '%s'\n", e.end_expr);
				e.stop = &find_charrange;
			}
			n_match = 0; /* number of type expression matches */
			num_o = 0;   /* number of expression specifiers found inside expr */
			/* try to find at most max_m expr */
			while (n_match < max_m) {
				res = match_expr(&e, in + j, o, &num_o);
				if (res == 0)
					break;
				j += res;
				n_match++;
				if (in[j] == '\0') {
					debug(SCANF, 1, "reached end of input scanning 'in'\n");
					break;
				}
			}
			debug(SCANF, 3, "Exiting loop with expr '%s' matched %d times, found %d objects\n", expr, n_match, num_o);
			if (n_match < min_m) {
				debug(SCANF, 1, "found expr '%s' %d times, but required %d\n", expr, n_match, min_m);
				return n_found;
			}
			/* if there was a conversion specifier, we must consume va_list */
			if (num_cs) {
				if (n_match > 1) {
					num_o /= n_match;
					debug(SCANF, 1, "conversion specifier matching in a pattern is supported only with '?'; restoring only %d found objects\n", num_o);
				}
				if (n_match) {
					consume_valist_from_object(o, num_o, ap);
					n_found += num_o;
				} else {
					/* 0 match but we had num_cs conversion specifier 
					   we must consume them */
					debug(SCANF, 4, "0 match but there was %d CS so consume them\n", num_cs);
					for (k = 0; k < num_cs; k++)
						o = va_arg(ap, struct sto *);
				}
			}
			i++;
			continue;
		}
		if (c == '\0' || in[j] == '\0') {
			return n_found;
		} else if (c == '%') {
			res = parse_conversion_specifier(in, fmt, &i, &j, o);

			if (res == 0)
				return n_found;
			consume_valist_from_object(o, 1, ap);
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
			i2 = strxcpy_until(expr, fmt + i, sizeof(expr), c2);
			if (i2 == -1) {
				debug(SCANF, 1, "Invalid format '%s', unmatched '%c'\n", fmt, c);
				return n_found;
			}
			debug(SCANF, 8, "found expr '%s'\n", expr);
			i += i2;
			if (is_multiple_char(fmt[i]))
				continue;
			i2 = 0;
			res = match_expr_single(expr, in + j, o, &i2);
			if (res == 0) {
				debug(SCANF, 1, "Expr'%s' didnt match in 'in' at offset %d\n", expr, j);
				return n_found;

			}
			j += res;
			consume_valist_from_object(o, i2, ap);
			debug(SCANF, 4, "Expr'%s' matched 'in' at offset %d, found %d objects\n", expr, j, i2);
			n_found += i2;
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
				debug(SCANF, 2, "in[%d]=%c, != fmt[%d]=%c, exiting\n",
						j, in[j], i, fmt[i]);
				return  n_found;
			}
			i++;
			j++;
		}

	} /* while 1 */
}

int st_sscanf(char *in, const char *fmt, ...) {
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = st_vscanf(in, fmt, ap);
	va_end(ap);
	return ret;
}

