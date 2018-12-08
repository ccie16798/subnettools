/*
 * strtok variant functions
 *
 * Copyright (C) 2018 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#include <string.h>
#include "st_strtok.h"

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

