/*
 * some "useful" functions
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

#define VAL4X    -1, -1, -1, -1
#define VAL16X   VAL4X, VAL4X, VAL4X, VAL4X
#define VAL64X   VAL16X, VAL16X, VAL16X, VAL16X
#define VAL256X  VAL64X, VAL64X, VAL64X, VAL64X

const int hex_tab[] = { VAL256X,
	['0'] = 0, ['1'] = 1, ['2'] = 2, ['3'] = 3, ['4'] = 4,
	['5'] = 5, ['6'] = 6, ['7'] = 7, ['8'] = 8, ['9'] = 9,
	['a'] = 10, ['b'] = 11, ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15,
	['A'] = 10, ['B'] = 11, ['C'] = 12, ['D'] = 13, ['E'] = 14, ['F'] = 15 };

const int int_tab[] = { VAL256X,
	['0'] = 0, ['1'] = 1, ['2'] = 2, ['3'] = 3, ['4'] = 4,
	['5'] = 5, ['6'] = 6, ['7'] = 7, ['8'] = 8, ['9'] = 9 };

int isUnsignedInt(const char *s)
{
	int i;

	if (*s == '\0')
		return 0;
	for (i = 1; ; i++) {
		if (!isdigit(s[i]))
			return 0;
		if (s[i] == '\0')
			break;
	}
	return 1;
}

int isInt(const char *s)
{
	int i;

	if (*s == '\0')
		return 0;
	if (*s != '-' && !isdigit(*s))
		return 0;
	if (s[0] == '-' && s[1] == '\0')
		return 0;
	for (i = 1; ; i++) {
		if (!isdigit(s[i]))
			return 0;
		if (s[i] == '\0')
			break;
	}
	return 1;
}

int string2int(const char *s, int *res)
{
	int i = 0, a = 0;
	int sign = 1;

	if (s == NULL) {
		*res = -2;
		return 0;
	}
	if (*s == '-') {
		sign = -1;
		i++;
	}
	if (s[i] == '\0') { /* don't interpret '' as zero */
		*res = -1;
		return 0;
	}
	while (s[i] != '\0') {
		if (!isdigit(s[i])) {
			*res = -1;
			return 0;
		}
		a *= 10;
		a += s[i] - '0';
		i++;
	}
	*res = 0;
	return a * sign;
}

int isPower2(unsigned int x)
{
	/* if x power of two, x - 1 will clear its only high bit and set all bits below to 1 */
	return x && !(x & (x - 1));
}

/* x must be power of 2 */
int mylog2(unsigned int x)
{
	int a = 0;

	while (x > 1) {
		x /= 2;
		a++;
	}
	return a;
}

unsigned nextPow2_32(unsigned x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}


unsigned long long nextPow2_64(unsigned long long x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	x++;
	return x;
}

char *remove_space(char *s)
{
	int i = 0, j = 0;

	while (isspace(*s))
		s++;
	while (1) {
		if (isspace(s[i])) {
			i++;
			continue;
		}
		if (i == j) {
			i++;
			j++;
			continue;
		}
		s[j] = s[i];
		if (s[i] == '\0')
			return s;
		i++;
		j++;
	}
}

void remove_ending_space(char *s)
{
	int j;

	for (j = strlen(s) - 1; j >= 0; j--) {
		if (!isspace(s[j]))
			return;
		s[j] = '\0';
	}
}

/* strlcpy */
int strxcpy(char *dst, const char *src, int size)
{
	int i = 0;

	while (1) {
		if (src[i] == '\0') {
			dst[i] = '\0';
			return i;
		}
		if (i == size - 1) {
			dst[i] = '\0';
			while (src[i] != '\0')
				i++;
			return i;
		}
		dst[i] = src[i];
		i++;
	}
}

int count_char(const char *s, char c)
{
	int i = 0, a = 0;

	while (1) {
		if (s[i] == '\0')
			return a;
		if (s[i] == c)
			a++;
		i++;
	}
}

int strxcpy_until(char *dst, const char *src, int n, char end)
{
	int i = 0;

	while (1) {
		if (src[i] == '\0' || i == n - 1) {
			dst[i] = '\0';
			return -1;
		}
		if (src[i] == end) {
			dst[i]     = end;
			dst[i + 1] = '\0';
			return i + 1;
		}
		dst[i] = src[i];
		i++;
	}
}

/* strtok variant ; treat consecutive delims one by one
 * standard strtok treats n successives delims as one,
 * which is not always what we want in CSV files
 */
char *st_strtok(char *s, const char *delim)
{
	int i;
	static char *s2;
	char *s3;

	if (s == NULL)
		s = s2;
	else
		s2 = s;
	s3 = s;

	if (s2 == NULL)
		return NULL;
	while (*s != '\0') {
		for (i = 0; i < strlen(delim); i++) {
			if (*s == delim[i]) {
				*s = '\0';
				s2 = s + 1;
				return s3;
			}
		}
		s++;
	}
	if (s3 == s)
		return NULL;
	s2 = NULL;
	return s3;
}

char *st_strtok_r(char *s, const char *delim, char **s2)
{
	int i;
	char *s3;

	if (s == NULL)
		s = *s2;
	else
		*s2 = s;
	if (*s2 == NULL)
		return NULL;
	s3 = s;
	while (*s != '\0') {
		for (i = 0; i < strlen(delim); i++) {
			if (*s == delim[i]) {
				*s = '\0';
				*s2 = s + 1;
				return s3;
			}
		}
		s++;
	}
	if (s3 == s)
		return NULL;
	*s2 = NULL;
	return s3;
}

/* just one delim */
char *st_strtok_r1(char *s, const char *delim, char **s2)
{
	char *s3;

	if (s == NULL)
		s = *s2;
	else
		*s2 = s;
	if (*s2 == NULL)
		return NULL;
	s3 = s;
	while (*s != '\0') {
		if (*s == *delim) {
			*s = '\0';
			*s2 = s + 1;
			return s3;
		}
		s++;
	}
	if (s3 == s)
		return NULL;
	*s2 = NULL;
	return s3;
}

char *fgets_truncate_buffer(char *buffer, int size, FILE *stream, int *res)
{
	char *s;
	int a, i, len;

	s = fgets(buffer, size, stream);
	*res = 0;
	/* not sure s[0] can be 0 here but better be safe than sorry*/
	if (s == NULL || s[0] == '\0')
		return s;
	i = 0;
	len = strlen(s) - 1;
	if (s[len] != '\n') { /* BFB; BIG FUCKING BUFFER; try to handle that  */
		while ((a = fgetc(stream)) != EOF) {
			i++;
			if (a == '\n')
				break;
		}
	} else
		s[len] = '\0'; /* remove next-line */
	*res = i;
	return s;
}

/* count number of consersion specifier in an expr
 * doesnt validate CS are valid
 */
int count_cs(const char *expr)
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
