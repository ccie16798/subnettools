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

int isUnsignedInt(const char *s)
{
	int i;

	if (*s == '\0')
		return 0;
	for (i = 0; i < strlen(s); i++) {
		if (!isdigit(s[i]))
			return 0;
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
	for (i = 1; i < strlen(s); i++) {
		if (!isdigit(s[i]))
			return 0;
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

inline int char2int(char c)
{
	if (c >= 'A' && c <= 'F') return (10 + c - 'A');
	if (c >= 'a' && c <= 'f') return (10 + c - 'a');
	if (c >= '0' && c <= '9') return (c - '0');
	return 0;
}

int isPower2(unsigned int x)
{
	while (((x % 2) == 0) && x > 1) /* if each time we divide x%2 == 0, x is power of two */
		x /= 2;
	return (x == 1);
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
		if (src[i] == '\0' || i == size - 1) {
			dst[i] = '\0';
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
 * standard strtok treats n successives delims as one, which is not always what we want in CSV files*/
char *simple_strtok(char *s, const char *delim)
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
	return NULL;
}

char *simple_strtok_r(char *s, const char *delim, char **s2)
{
	int i;
	char *s3;

	if (s == NULL)
		s = *s2;
	else
		*s2 = s;
	s3 = s;

	if (*s2 == NULL)
		return NULL;

	while (*s != '\0') {
		for (i = 0; i < strlen(delim); i++) {
			if (*s == delim[i]) {
				*s  ='\0';
				*s2 = s + 1;
				return s3;
			}
		}
		s++;
	}
	return NULL;
}

char *fgets_truncate_buffer(char *buffer, int size, FILE *stream, int *res)
{
	char *s;
	int a, i;

	s = fgets(buffer, size, stream);
	*res = 0;
	if (s == NULL || s[0] == '\0') /* not sure s[0] can be 0 here but better be safe than sorry*/
		return s;
	i = 0;
	if (s[strlen(s) - 1] != '\n') { /* BFB; BIG FUCKING BUFFER; try to handle that  */
		while ((a = fgetc(stream)) != EOF) {
			i++;
			if (a == '\n')
				break;
		}
	}
	*res = i;
	return s;
}
