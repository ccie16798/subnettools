#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "utils.h"
#include "iptools.h"
#include "debug.h"

struct expr {
	int num_expr;
	char *expr[10];
	int (*stop)(char *remain, struct expr *e);
	char end_of_expr; /* if remain[i] = end_of_expr , we can stop*/
	int stop_on_nomatch; /* we stop the match in case match_expr_simple doesnt match */
};

int match_expr_simple(char *expr, char *in, va_list *ap);

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

static int max_match(char c) {
	if (c == '*') return 1000000000;
	if (c == '+') return 1000000000;
	if (c == '?') return 1;
	return 0;
}

static int min_match(char c) {
	if (c == '*') return 0;
	if (c == '+') return 1;
	if (c == '?') return 0;
	return 0;
}

/* given an expression e, the following function 
 * try to determine if the remaining chars match their type
 * for exemple, find_int will match on the first digit found
*/
int find_int(char *remain, struct expr *e) {
	return isdigit(*remain) || (*remain == '-' && isdigit(remain[1]));
}

int find_word(char *remain, struct expr *e) {
	return isalpha(*remain);
}

/* fmt = FORMAT buffer
   in  = input buffer
   i   = index in fmt
   j   = index in in
*/
int parse_conversion_specifier(char *in, const char *fmt, int *i, int *j, va_list *ap) {
	int n_found = 0;
	int j2, res;
	int max_field_length;
	char buffer[128];
	char c;
	struct subnet *v_sub;
	struct ip_addr *v_addr;
	char *v_s;
	long *v_long;
	int *v_int;
	int sign;

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
		debug(SCANF, 3, "Found max field length %d\n", max_field_length);
	} else
		max_field_length = sizeof(buffer) - 2;
	switch (fmt[*i + 1]) {
		case '\0':
			debug(SCANF, 1, "Invalid format string '%s', ends with %%\n", fmt);
			return n_found;
		case 'P':
			v_sub = va_arg(*ap, struct subnet *);
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
				debug(SCANF, 2, "'%s' is a valid IP\n", buffer);
				n_found++;
			} else { 
				debug(SCANF, 2, "'%s' is an invalid IP\n", buffer);
				return n_found;
			}
			break;
		case 'I':
			v_addr = va_arg(*ap, struct ip_addr *);
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
				debug(SCANF, 2, "'%s' is a valid IP\n", buffer);
				n_found++;
			} else {
				debug(SCANF, 2, "'%s' is an invalid IP\n", buffer);
				return n_found;
			}
			break;
		case 'M':
			v_int = va_arg(*ap, int *);
			while ((isdigit(in[j2]) || in[j2] == '.') && j2 - *j < max_field_length) {
				buffer[j2 - *j] = in[j2];
				j2++;
			}
			buffer[j2 - *j] = '\0';
			if (j2 - *j == 0) {
				debug(SCANF, 2, "no MASK found at offset %d\n", *j);
				return n_found;
			}
			debug(SCANF, 2, "possible MASK '%s' starting at offset %d\n", buffer, *j);
			res = string2mask(buffer, 21);
			if (res != BAD_MASK) {
				debug(SCANF, 2, "'%s' is a valid MASK\n", buffer);
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

			}
			v_long = va_arg(*ap, long *);
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
			debug(SCANF, 2, "found LONG '%ld' at offset %d\n", *v_long, *j);
			n_found++;
			break;
		case 'd':
			v_int = va_arg(*ap, int *);
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
			debug(SCANF, 2, "found INT '%d' at offset %d\n", *v_int, *j);
			n_found++;
			break;
		case 's':
			v_s = va_arg(*ap, char *);
			c = fmt[*i + 2];
			if (c == '.') {
				debug(SCANF, 1, "Invalid format '%s', found '.' after %%s\n", fmt);
				return n_found;
			} else if (c == '%') {
				debug(SCANF, 1, "Invalid format '%s', found '%%' after %%s\n", fmt);
				return n_found;
			}
			while (in[j2] != c && j2 - *j < max_field_length - 1) {
				if (in[j2] == '\0')
					break;
				v_s[j2 - *j] = in[j2];
				j2++;
			}
			v_s[j2 - *j] = '\0';
			debug(SCANF, 2, "found STRING '%s' starting at offset %d \n", v_s, *j);
			n_found++;
			break;
		case 'W':
			v_s = va_arg(*ap, char *);
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
		case 'c':
			v_s = va_arg(*ap, char *);
			v_s[0] = in[*j];
			debug(SCANF, 5, "CHAR '%c' found at offset %d\n", *v_s,  *j);
			j2 = *j + 1;
			n_found++;
		default:
			break;
	} /* switch */
	*j = j2;
	*i += 2;
	return n_found;
}

/*
 * match a single pattern 'expr' against 'in'
 * returns 0 if doesnt match, number of matched char  in input buffer 
 */
int match_expr_simple(char *expr, char *in, va_list *ap) {
	int i, j;
	int a = 0;
	int res;
	char c, low, high;

	i = 0; /* index in expr */
	j = 0; /* index in input buffer */

	while (1) {
		debug(SCANF, 5, "remaining   in='%s'\n", in);
		debug(SCANF, 5, "remaining expr='%s'\n", expr);
		if (in[j] == '\0' || expr[i] == '\0')
			return a;
		c = expr[i];
		switch (c) {
			case '.':
				i++;
				j++;
				a++;
				break;
			case '[': /* try to handle char range like [a-Zbce-f] */
				i++;
				res = 0;
				while (expr[i] != ']') {
					low = expr[i];
					if (expr[i + 1] == '-') {
						high = expr[i + 2];
						if (high == '\0' || high == ']') {
							debug(SCANF, 1, "Invalid expr '%s', incomplete range\n", expr);
							return a;
						}
						if (in[j] >= low && in[j] <= high)
							res = 1;
					} else {
						if (low == in[j])
							res  = 1;
					}
					i++;
				}
				i++;
				j++;
				if (res)
					a++;
				break;
			case '\\':
				i++;
				c = escape_char(expr[i]);
			case '%':
				debug(SCANF, 1, "Need to find conversion specifier\n");
				res = parse_conversion_specifier(in, expr, &i, &j, ap);
				break;	
			default:
				if (in[j] != c)
					return 0;
				i++;
				j++;
				a++;
				break;
		}

	}
}
/* match expression e against input buffer 
 * will return 
 * -1 if it doesnt match  
 * 0 if it doesnt match but we found the character after the expr
 * n otherwise
 */
int match_expr(struct expr *e, char *in, va_list *ap) {
	int i = 0;
	int res = 0;
	int res2;

	for (i = 0; i < e->num_expr; i++) {
		res = match_expr_simple(e->expr[i], in, ap);
		debug(SCANF, 4, "Matching expr '%s' against input '%s' res=%d\n", e->expr[i], in, res);
		if (res)
			break;
	}	
	if (e->stop) {
		res2 = e->stop(in, e);
		debug(SCANF, 4, "trying to stop on '%s', res=%d\n", in, res2);
	} else {
		if (e->stop_on_nomatch)
			res2 = !res;
		else
			res2 = (*in == e->end_of_expr);
		debug(SCANF, 4, "trying to stop on '%c', res=%d\n", e->end_of_expr, res2);
	}
	if (res2)
		return 0;
	else if (res == 0)
		return -1;
	else
		return res;
}

int find_ip(char *remain, struct expr *e) {
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

static int st_vscanf(char *in, const char *fmt, va_list ap) {
	int i, j, i2;
	int res;
	int min_m, max_m;
	int n_found, n_match;
	char c;
	char expr[64];
	struct expr e;

	i = 0; /* index in fmt */
	j = 0; /* index in in */
	n_found = 0; /* number of arguments found */
	expr[0] = '\0';
	while (1) {
		c = fmt[i];
		debug(SCANF, 4, "Still to parse in FMT  : %s\n", fmt + i);
		debug(SCANF, 4, "Still to parse in 'in' : %s\n", in + j);
		if (is_multiple_char(c)) {
			min_m = min_match(c);
			max_m = max_match(c);
			debug(SCANF, 4, "need to find expression '%s' %c time\n", expr, c);
			e.expr[0] = expr;
			e.num_expr = 1;
			e.stop_on_nomatch = 0;
			e.end_of_expr = fmt[i + 1]; /* if necessary */
			e.stop = NULL;
			if (fmt[i + 1] == '%') {
				if (fmt[i + 2] == '\0') {
					debug(SCANF, 1, "Invalid format string '%s', ends with %%\n", fmt);
					return n_found;
				} else if (fmt[i + 2] == 'd') {
					e.stop = &find_int;
				} else if (fmt[i + 2] == 'I') {
					e.stop = &find_ip;
				} else if (fmt[i + 2] == 'P') {
					e.stop = &find_ip;
				} else if (fmt[i + 2] == 'M') {
					e.stop = &find_int;
				} else if (fmt[i + 2] == 'W') {
					e.stop = &find_word;
				} else if (fmt[i + 2] == 's') {
					e.stop_on_nomatch = 1;
				} else if (fmt[i + 2] == 'c') {
					e.stop_on_nomatch = 1;
				}
			}
			n_match = 0;
			while (n_match < max_m) {
				res = match_expr(&e, in + j, &ap);
				if (res == -1) {
					debug(SCANF, 1, "No match found for expr '%s'\n", expr);
					return n_found;
				}
				if (res == 0)
					break;
				j += res;
				n_match++;
			}
			debug(SCANF, 3, "Exiting loop with expr '%s' matched %d times\n", expr, n_match); 
			if (n_match < min_m) {
				debug(SCANF, 1, "Couldnt find expr '%s', exiting\n", expr);
				return n_found;
			}		
			i++;
			continue;
		} 
		if (c == '\0' || in[j] == '\0') {
			return n_found;
		} else if (c == '%') {
			res = parse_conversion_specifier(in, fmt, &i, &j, &ap);
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
			debug(SCANF, 2, "fmt[%d]='.', match any char\n", i);
			i++;
			j++;
		} else if (c == '(' || c == '[') {
			char c2 = (c == '(' ? ')' : ']');
			debug(SCANF, 2, "fmt[%d]=%c, waiting expr\n", i, c);
			i2 = i;
			while (fmt[i2] != c2) {
				expr[i2 - i] = fmt[i2];
				if (fmt[i2] == '\0') {
					debug(SCANF, 1, "Invalid format '%s', unmatched '%c'\n", fmt, c);
					return n_found;
				}
				i2++;
			}
			expr[i2 - i] = c2;
			i2++;
			expr[i2 - i] = '\0';
			debug(SCANF, 2, "found expr '%s'\n", expr);
			i = i2;
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

