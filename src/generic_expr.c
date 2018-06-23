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
#include <ctype.h>
#include "debug.h"
#include "generic_expr.h"
#include "utils.h"


static inline int is_comp(char c)
{
	return ((c == '=') | (c == '<') | (c == '>') | (c == '~') |
			(c == '{') | (c == '}') | (c == '#') | (c == '%'));
}

void init_generic_expr(struct generic_expr *e, const char *s,
		int (*compare)(const char *, const char *, char, void *))
{
	e->pattern = s;
	if (s == NULL)
		return;
	e->pattern_len = strlen(s);
	e->recursion_level = 0;
	e->compare = compare;
}

/*
 * simple_expr: evaluate a simple expression
 * no OR '|', no AND '&', no negate '!', just 'A COMPARATOR B' is evaluated
 * @pattern : the expression to evaluate
 * @len	    : the length of the expression
 * @e       : a struct containing compare function, recursion level
 * returns:
 * 	-1 : invalid expression
 *  	1  : true
 *  	0  : false
 */
static int simple_expr(char *pattern, int len, struct generic_expr *e)
{
	int i = 0, j = 0;
	char operator, c;
	int res;

	e->recursion_level++;
	if (e->recursion_level >= GENERIC_ST_MAX_RECURSION) {
		debug(GEXPR, 1, "Invalid expr '%s', too many recursion level\n", e->pattern);
		return -1;
	}
	/* first try to find the comparator in the pattern */
	while (1) {
		if (pattern[i] == '\0' || i == len) {
			debug(GEXPR, 1, "Invalid expr '%s', no comparator\n", pattern);
			e->recursion_level--;
			return -1;
		}
		if (is_comp(pattern[i]))
			break;
		i++;
	}
	operator = pattern[i];
	pattern[i] = '\0';
	i++;
	j = i; /* j now points to the second part of the expression */
	while (1) {
		if (pattern[i] == '\0' || i == len)
			break;
		if (is_comp(pattern[i]) && pattern[i - 1] != '\\') {
			pattern[j - 1] = operator;
			debug(GEXPR, 1, "Invalid expr '%s', 2 x comparators\n",
					pattern);
			e->recursion_level--;
			return -1;
		}
		i++;
	}
	c = pattern[i];
	pattern[i] = '\0';
	res = e->compare(pattern, pattern + j, operator, e->object);
	debug(GEXPR, 5, "comparing '%s' against '%s', return=%d\n", pattern, pattern + j, res);
	e->recursion_level--;
	/* restoring pattern to its original value */
	pattern[j - 1] = operator;
	pattern[i]     = c;
	return res;
}

/*
 * run_generic_expr: evaluate a complex expression
 * can contain multiples OR '|' , AND '&', NEGATE '!'
 * if sub-expression is found, it will use recursion until it finds
 * a 'simple' expression
 * @pattern : the expression to evaluate
 * @len	    : the length of the expression
 * @e       : a struct containing compare function, recursion level
 * returns:
 * 	-1 : invalid expression
 *  	1  : true
 *  	0  : false
 */
int run_generic_expr(char *pattern, int len, struct generic_expr *e)
{
	int i = 0, j;
	char buffer[256];
	int res1, res2;
	int parenthese = 0;
	int negate = 0;

	e->recursion_level++;
	if (e->recursion_level >= GENERIC_ST_MAX_RECURSION) {
		debug(GEXPR, 1, "Invalid expr '%s', too many recursion level\n",
				e->pattern);
		return -1;
	}
	/* FIXME this should die once code is stabilized */
	strxcpy(buffer, pattern, sizeof(buffer));
	debug(GEXPR, 9, "Pattern : '%s', len=%d, recursion=%d\n",
			buffer, len, e->recursion_level);

	while (isspace(pattern[i]))
		i++;
	if (pattern[i] == '!') {
		negate++;
		i++;
	}
	if (i >= len) {
		/* it should occur only on BUG*/
		fprintf(stderr, "%s: BUG i=%i, len=%d\n", __func__, i, len);
		return -1;
	}
	/* handle expr inside parenthesis */
	if (pattern[i] == '(') {
		debug(GEXPR, 5, "Found a '(', trying to split expr\n");
		i += 1;
		j = i; /* j is set to the start of tentative sub-expression */
		parenthese++;
		while (1) {
			if (pattern[i] == '\0' || i == len) {
				debug(GEXPR, 1, "Invalid pattern '%s', no closing ')'\n",
						pattern);
				return -1;
			}
			if (pattern[i] == '(')
				parenthese++;
			else if (pattern[i] == ')' && parenthese == 1) {
				debug(GEXPR, 5, "Found closing ')', recursion\n");
				res1 = run_generic_expr(pattern + j,  i - j, e);
				e->recursion_level--;
				if (res1 < 0)
					return res1;
				/*
				 * negate applies only to the expression in parenthesis
				 * it is stronger than '&' and '|'
				 */
				res1 = (negate ? !res1 : res1);
				i++;
				while (isspace(pattern[i]))
					i++;
				if (i > len) {
					/* it should occur only on BUG*/
					fprintf(stderr, "%s: BUG i=%i, len=%d\n",
							__func__, i, len);
					return -1;
				}
				/* we reached end of string, just return */
				if (pattern[i] == '\0' || len == i)
					return res1;
				/* take shortcuts to avoid evalutating second part of expr
				 * downside is that if the other part of expr has a syntax error,
				 * we don't catch it
				 */
				if (res1 && pattern[i] == '|')
					return 1;
				if (res1 == 0 && pattern[i] == '&')
					return 0;
				/* calling recursion on second part of expression */
				res2 = run_generic_expr(pattern + i + 1, len - i - 1, e);
				e->recursion_level--;
				if (res2 < 0)
					return res2;
				if (pattern[i] == '|')
					return res1 | res2;
				else if (pattern[i] == '&')
					return res1 & res2;
				debug(GEXPR, 1, "'%s' invalid, a comparator is required after ')'\n",
						buffer);
				return -1;
			} else if (pattern[i] == ')')
				parenthese--;
			i++;
		}
	}
	j = i;
	while (1) {
		if (i == len || pattern[i] == '\0')
			break;
		if (pattern[i] == '|') {
			res1 = run_generic_expr(pattern + j, i - j, e);
			e->recursion_level--;
			if (res1 < 0)
				return res1;
			res1 = (negate ? !res1 : res1);
			if (res1) /* shortcut, no need to evaluate other side */
				return res1;
			res2 = run_generic_expr(pattern + i + 1, len - i - 1, e);
			e->recursion_level--;
			if (res2 < 0)
				return res2;
			/*
			 * negate doesnt apply to the  whole expression
			 */
			return (res1 | res2);
		}
		if (pattern[i] == '&') {
			res1 = run_generic_expr(pattern + j, i - j, e);
			e->recursion_level--;
			if (res1 < 0)
				return res1;
			res1 = (negate ? !res1 : res1);
			if (res1 == 0) /* shortcut */
				return res1;
			res2 = run_generic_expr(pattern + i + 1, len - i - 1, e);
			e->recursion_level--;
			if (res2 < 0)
				return res2;
			return (res1 & res2);
		}
		i++;
	}
	res1 = simple_expr(pattern + j, i - j, e);
	e->recursion_level--;
	if (res1 < 0)
		return res1;
	return negate ? !res1 : res1;
}

/* used for testing purposes */
int int_compare(const char *s1, const char *s2, char o, void *object)
{
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
