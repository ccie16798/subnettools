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

int isUnsignedInt(const char *s) {
	int i;
	for (i = 0; i < strlen(s); i++) {
		if (!isdigit(s[i]))
			return 0;
	}
	return 1;
}

int isInt(const char *s) {
	int i;
	if (*s != '-' && !isdigit(*s))
		return 0;
	for (i = 1; i < strlen(s); i++) {
		if (!isdigit(s[i]))
			return 0;
	}
	return 1;
}

int char2int(char c) {
	if (c >= 'A' && c <= 'F') return (10 + c - 'A');
	if (c >= 'a' && c <= 'f') return (10 + c - 'a');
	if (c >= '0' && c <= '9') return (c - '0');
	return 0;
}

int isPower2 (unsigned int x) {
	while (((x % 2) == 0) && x > 1) /* if each time we divide x%2 == 0, x is power of two */
		x /= 2;
	return (x == 1);
}
/* x must be power of 2 */
int mylog2(unsigned int x) {
	int a = 0;
	while (x > 1) {
		x /= 2;
		a++;
	}
	return a;
}

/* strtok variant ; treat consecutive delims one by one
 * standard strtok treats n successives delims as one, which is not always what we want in CSV files*/
char *simple_strtok(char *s, const char *delim) {
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
				//printf("s2 %s\n",s2);
				//printf("s3 %s\n",s3);
				return s3;
			}
		}
		s++;
	}
	return NULL;
}

char *simple_strtok_r(char *s, const char *delim, char **s2) {
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
				*s='\0';
				*s2 = s + 1;
				//printf("s2 %s\n",s2);
				//printf("s3 %s\n",s3);
				return s3;
			}
		}
		s++;
	}
	return NULL;
}

/* strlcpy */
char *strxcpy(char *dest, const char *src, int size) {
	strncpy(dest, src, size);
	dest[size - 1] = '\0';
	return dest;
}


char *fgets_truncate_buffer(char *buffer, int size, FILE *stream, int *res) {
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
