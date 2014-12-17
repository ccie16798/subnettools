/*
 * bitmap shift functions (to compare IPv6 addresses)
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

void shift_right(MYTYPE *buffer, int len, int shift) {
	int shift1, shift2;
	int i;
	unsigned short reminder;

	shift1 = shift / TYPE_SIZE;
	shift2 = shift % TYPE_SIZE;

	for (i = len - 1; i >= shift1; i--)
		buffer[i] = buffer[i - shift1];
	for ( ; i >= 0; i--)
		buffer[i] = 0;
	if (shift2 == 0)
		return;

	for (i = len - 1; i > 0; i--) {
		buffer[i] >>= shift2;
		reminder = buffer[i - 1] << (TYPE_SIZE - shift2);
		buffer[i] |= reminder;
	}
	buffer[0] >>= shift2;
}



void shift_left(MYTYPE *buffer, int len, int shift) {
        int shift1, shift2;
        int i;
        unsigned short reminder;

        shift1 = shift / TYPE_SIZE;
        shift2 = shift % TYPE_SIZE;

	for (i = 0; i < len - shift1; i++)
		buffer[i] = buffer[i + shift1];
	for ( ; i < len; i++)
		buffer[i] = 0;
	if (shift2 == 0)
		return;
	for (i = 0; i < len - 1; i++) {
                buffer[i] <<= shift2;
                reminder = buffer[i + 1] >> (TYPE_SIZE - shift2);
                buffer[i] |= reminder;
        }
	buffer[len - 1] <<= shift2;
}

void print_bitmap(MYTYPE *buffer, int len) {
	int i;
	int i1, i2;
	int a;
	for (i = 0; i < len * TYPE_SIZE; i++) {
		i1 = i / TYPE_SIZE;
		i2 = TYPE_SIZE - 1 - i % TYPE_SIZE;
		a = !! ((buffer[i1]) & (1 << i2));
		printf("%d", a);
	}
	printf("\n");
}

int sprint_bitmap(char *outbuf, MYTYPE *buffer, int len) {
	int i;
        int i1, i2;
        int a, b = 0;

        for (i = 0; i < len * TYPE_SIZE; i++) {
		i1 = i / TYPE_SIZE;
		i2 = TYPE_SIZE - 1 - i % TYPE_SIZE;
                a = !! ((buffer[i1]) & (1 << i2));
                b += sprintf(outbuf++, "%d", a);
        }
	*outbuf = '\0';
	return b;
}

void increase_bitmap(MYTYPE *buffer, int len) {
	int i = len - 1;

	for (  ; i >= 0; i--) {
		if (buffer[i] != TYPE_MAX)
			break;
		else
			buffer[i] = 0;
	}
	if (i >= 0)
		buffer[i]++;
}

void decrease_bitmap(MYTYPE *buffer, int len) {
	int i = len - 1;

	for (  ; i >= 0; i--) {
		if (buffer[i] != 0)
			break;
		else
			buffer[i] = TYPE_MAX;
	}
	if (i >= 0)
		buffer[i]--;
}
/*
int main(int argc, char **argv) {
	unsigned short  *buffer;
	int i;
	int len = strlen(argv[1])/2;

	buffer = (unsigned short *)argv[1];
	print_bitmap(buffer, len);

	for (i=0; i< 32; i++) {
		shift_left(buffer, len, 6 );
		print_bitmap(buffer, len);
	}
	for (i=0; i< 32; i++) {
		shift_right(buffer, len, 6 );
		print_bitmap(buffer, len);
	}
	shift_right(buffer, len, atoi(argv[2]) );
	print_bitmap(buffer, len);


}
*/
