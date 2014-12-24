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
	int (*stop)(char *remain, struct  expr *e);
	char end_of_expr;
};

int find_int(char *remain, struct expr *e) {
	return isdigit(*remain) || (*remain == '-' && isdigit(remain[1]));
}

int find_word(char *remain, struct expr *e) {
	return isalpha(*remain);
}

int find_string(char *remain, struct expr *e) {
	return 0;
}

int find_char(char *remain, struct expr *e) {
	int i, res;

	for (i = 0; i < e->num_expr; i++) {
		res = match_expr_simple(e->expr[i], remain);
		if (res)
			break;
	}	
	return !res;
}
/*
 * return 0 if no match
 */
int match_expr_simple(char *expr, char *in) {
	int i, j;
	int a = 0;

	i = 0; /* index in expr */
	j = 0; /* index in input buffer */

	while (1) {
		if (in[j] == '\0' || expr[i] == '\0')
			return a;
		switch (expr[i]) {
			case '.':
				i++;
				j++;
				a++;
				break;
			default:
				if (in[j] != expr[i])
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
int match_expr(struct expr *e, char *in) {
	int i = 0;
	int res = 0;
	int res2;

	for (i = 0; i < e->num_expr; i++) {
		res = match_expr_simple(e->expr[i], in);
		debug(SCANF, 4, "Matching expr '%s' against input '%s' res=%d\n", e->expr[i], in, res);
		if (res)
			break;
	}	
	if (e->stop) {
		res2 = e->stop(in, e);
		debug(SCANF, 4, "trying to stop on '%s', res=%d\n", in, res2);
	} else {
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

	while (is_ip_char(remain[i])) {
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
	int i, j, j2, i2;
	int res;
	int n_found, n_match;
	char c;
	char buffer[128];
	char expr[64];
	struct expr  e;
	struct subnet *v_sub;
	long *v_int;
	char *v_s;

	i = 0; /* index in fmt */
	j = 0; /* index in in */
	n_found = 0; /* number of arguments found */
	expr[0] = '\0';
	while (1) {
		c = fmt[i];
		debug(SCANF, 4, "Still to parse in FMT  : %s\n", fmt + i); 
		debug(SCANF, 4, "Still to parse in 'in' : %s\n", in + j);
		if (c == '*') {
			debug(SCANF, 4, "need to find '%s' * time\n", expr);
			e.expr[0] = expr;
			e.num_expr = 1;
			e.end_of_expr = fmt[i + 1]; /* if necessary */
			if (fmt[i + 1] == '%') {
				if (fmt[i + 2] == '\0') {
					debug(SCANF, 1, "Invalid format string '%s', ends with %%\n", fmt);
					return n_found;
				} else if (fmt[i + 2] == 'd') {
					e.stop = &find_int;
				} else if (fmt[i + 2] == 'I') {
					e.stop = &find_ip;
				} else if (fmt[i + 2] == 'W') {
					e.stop = &find_word;
				} else if (fmt[i + 2] == 's') {
					e.stop = &find_string;
				} else if (fmt[i + 2] == 'c') {
					e.stop = &find_char;
				}
			} else
				e.stop = NULL;
			n_match = 0;
			while (1) {
				res = match_expr(&e, in + j);
				if (res == -1) {
					debug(SCANF, 1, "No match found for expr '%s'\n", expr);
					return n_found;
				}
				if (res == 0)
					break;
				j += res;
				n_match++;
			}
			debug(SCANF, 3, "Exiting loop with expr '%s' matched %d times\n", expr, n_match++); 
			i++;
			continue;
		} 
		if (c == '\0' || in[j] == '\0') {
			return n_found;
		} else if (c == '%') {
			j2 = j;
			switch (fmt[i + 1]) {
				case '\0':
					debug(SCANF, 1, "Invalid format string '%s', ends with %%\n", fmt);
					return n_found;
				case 'I':
					v_sub = va_arg(ap, struct subnet *);
					while (is_ip_char(in[j2])) {
						buffer[j2 - j] = in[j2];
						j2++;
					}
					buffer[j2 - j] = '\0';
					if (j2 - j <= 2) {
						debug(SCANF, 2, "no IP found at offset %d\n", j);
						return n_found;
					}
					debug(SCANF, 2, "possible IP %s starting at offset %d \n", buffer, j);
					res = get_subnet_or_ip(buffer, v_sub);
					if (res < 1000) {
						debug(SCANF, 2, "'%s' is a valid IP\n", buffer);
						n_found++;
					} else { 
						debug(SCANF, 2, "'%s' is an invalid IP\n", buffer);
						return n_found;
					}
					break;
				case 'd':
					v_int = va_arg(ap, long *);
					if (in[j] == '-') {
						buffer[0] = '-';
						j2++;
					}
					while (isdigit(in[j2])) {
						buffer[j2 - j] = in[j2];
						j2++;
					}
					buffer[j2 - j] = '\0';
					if (j == j2 || !strcmp(buffer, "-")) {
						debug(SCANF, 2, "no INT found at offset %d \n", j);
					} else {
						debug(SCANF, 2, "found INT %s starting at offset %d \n", buffer, j);
						*v_int = atol(buffer);
						n_found++;
					}
					break;
				case 's':

					break;
				case 'W':
					v_s = va_arg(ap, char *);
					j2 = j;
					while (isalpha(in[j2])) {
						v_s[j2 - j] = in[j2];
						j2++;
					}
					if (j2 == j) {
						debug(SCANF, 2, "no WORD found at offset %d\n", j);
						return n_found;
					}
					debug(SCANF, 5, "WORD '%s' found at offset %d\n", v_s,  j);
					v_s[j2 - j] = '\0';
					n_found++;
					break;
				case 'c':
					v_s = va_arg(ap, char *);
					v_s[0] = in[j];
					debug(SCANF, 5, "CHAR '%c' found at offset %d\n", *v_s,  j);
					j2 = j + 1;
				default:
					break;
			}
			i += 2;
			j = j2;
		} else if (c == '\\') { /* handling special chars */
			switch (fmt[i + 1]) {
				case '\0':
					debug(SCANF, 2, "Nul char after \\ \n");
					break;
				case 't':
					c = '\t';
					i++;
					break;
				case ' ':
					c = ' ';
					i++;
					break;
				case '.':
					c = '.';
					i++;
					break;
				case '*':
					c = '*';
					i++;
					break;
				case '+':
					c = '+';
					i++;
					break;
				default:
					c = '\\';
					break;
			}
			if (fmt[i + 2] == '*') {
				expr[0] = c;	
				expr[1] = '\0';	
				i++;
				continue;
			}
			if (in[j] != c) {
				debug(SCANF, 2, "in[%d]=%c, != fmt[%d]=%c, exiting\n",
						j, in[j], i, c);
				return  n_found;
			}
			i++;
			j++;
		} else if (c == '.') {
			if (fmt[i + 1] == '*') {
				expr[0] = c;	
				expr[1] = '\0';	
				i++;
				continue;
			}
			debug(SCANF, 2, "fmt[%d]='.', match any char\n", i);
			i++;
			j++;
		} else if (c == '(') {
			debug(SCANF, 2, "fmt[%d]='(', waiting expr\n", i);
			i2 = i + 1;
			while (fmt[i2] != ')') {
				expr[i2 - i - 1] = fmt[i2];
				if (fmt[i2] == '\0') {
					debug(SCANF, 2, "unmatched (\n");
					return n_found;
				}
				i2++;
			}
			expr[i2 - i - 1] = '\0';
			debug(SCANF, 2, "found expr %s\n", expr);
			i = i2 + 1;
		} else {
			if (fmt[i + 1] == '*') {
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

