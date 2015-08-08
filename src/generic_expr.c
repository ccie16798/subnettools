/*
 * Generic expression matching
 *
 * Copyright (C) 2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "generic_expr.h"
#include "utils.h"


static inline int is_and_or(char c) {
	return (c == '|') | (c == '&');
}

static inline int is_comp(char c) {
	return (c == '=') | (c == '<') | (c == '>') ;
}

int simple_expr(char *pattern, int len, struct generic_expr *e) {
	int i = 0, j;
	char string[256];
	char value[256];
	char operator;

	while (1) {
		if (pattern[i] == '\0' || i == len) {
			debug(GEXPR, 1, "Invalid expr '%s', no comparator\n", pattern);
			return -1;
		}
		if (is_comp(pattern[i]))
			break;
		string[i] = pattern[i];	
		i++;
	}
	operator = pattern[i];
	string[i] = '\0';
	i++;
	j = i;
	while (1) {
		if (pattern[i] == '\0' || i == len)
			break;
		if (is_comp(pattern[i])) {
			debug(GEXPR, 1, "Invalid expr '%s', 2 x comparators\n", pattern);
			return -1;
		}
		value[i - j] = pattern[i];
		i++;
	}
	value[i - j] = '\0';
	debug(GEXPR, 5, "comparing '%s' against '%s'\n", string, value);
	return e->compare(string, value, operator);	
}


int run_generic_expr(char *pattern, int len, struct generic_expr *e) {
	int i;
	char buffer[128];
	int res1, res2;
	int parenthese = 0;

	strxcpy(buffer, pattern, len + 1);
	debug(GEXPR, 9, "Pattern : '%s', len=%d\n", buffer, len);
	
	if (pattern[0] == '(') {
		debug(GEXPR, 3, "Found a '(', trying to split expr\n");
		i = 1;
		parenthese++;
		while (1) {
			if (pattern[i] == '\0' || i == len) {
				debug(GEXPR, 1, "Invalid pattern '%s'\n", pattern);
				return -1;
			}
			if (pattern[i] == '(')
				parenthese++;
			if (pattern[i] == ')' && parenthese == 1) {
				debug(GEXPR, 3, "Found closing '(', creating new expri '%s'\n", buffer);
				res1 = run_generic_expr(pattern + 1,  i - 1, e);
				res2 = run_generic_expr(pattern + i + 2,  len - i - 2, e);
				if (pattern[i + 1] == '|')
					return res1 | res2;
				else if (pattern[i + 1] == '&')
					return res1 & res2;
				debug(GEXPR, 3, "A comparator is required after ) '%s'\n", buffer);
				return -1;
			} else if (pattern[i] == ')')
				parenthese--;
			i++;
		}
	}
	i = 0;
	while (1) {
		if (i == len || pattern[i] == '\0')
			break;
		if (pattern[i] == '|') {
			res1 = run_generic_expr(pattern, i, e);
			res2 = run_generic_expr(pattern + i + 1, len - i - 1, e);
			return res1 | res2;
		}
		if (pattern[i] == '&') {
			res1 = run_generic_expr(pattern, i, e);
			res2 = run_generic_expr(pattern + i + 1, len - i - 1, e);
			return res1 & res2;
		}
		i++;
	}
	return simple_expr(pattern, i, e);
}

int int_compare(char *s1, char *s2, char o) {
	int l1 = atoi(s1);
	int l2 = atoi(s2);

	if (o == '=')
		return (l1 == l2);
	if (o == '<')
		return (l1 < l2);
	if (o == '>')
		return (l1 > l2);
	return 0;
}

#ifdef TEST
int main(int argc, char **argv) {
	struct generic_expr e;
	int res;	
	e.compare = int_compare;
	
	res = simple_expr(argv[1], strlen(argv[1]), &e);
	printf("result = %d\n", res);
}

#endif
