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


void shift_right(unsigned  short *buffer, int len, int shift) {
	int shift1;
	int shift2;
	int i;
	unsigned short reminder;
	shift1 = shift / 16;
	shift2 = shift % 16;

	for (i = len - 1; i >= shift1; i--)
		buffer[i] = buffer[i - shift1];
	for ( ; i >= 0; i--)
		buffer[i] = 0;
	if (shift2 == 0)
		return;

	for (i = len - 1; i > 0; i--) {
		buffer[i] >>= shift2;
		reminder = buffer[i - 1] << (16 - shift2);
		buffer[i] |= reminder;
	}
	buffer[0] >>= shift2;
}



void shift_left(unsigned  short *buffer, int len, int shift) {
        int shift1;
        int shift2;
        int i;
        unsigned short reminder;
        shift1 = shift / 16;
        shift2 = shift % 16;

	for (i = 0; i < len - shift1; i++)
		buffer[i] = buffer[i + shift1];
	for ( ; i < len; i++)
		buffer[i] = 0;
	if (shift2 == 0)
		return;
	for (i = 0; i < len - 1; i++) {
                buffer[i] <<= shift2;
                reminder = buffer[i + 1] >> (16 - shift2);
                buffer[i] |= reminder;
        }
	buffer[len - 1] <<= shift2;
}
void print_bitmap(unsigned  short *buffer, int len) {
	int i;
	int i1, i2;
	int a;
	for (i = 0; i < len * 16; i++) {
		i1 = i / 16;
		i2 = 15 - i % 16;
		a = !! ((buffer[i1]) & (1 << i2));
		printf("%d", a);
	}
	printf("\n");
}

int sprint_bitmap(char *outbuf, unsigned  short *buffer, int len) {
	int i;
        int i1, i2;
        int a, b = 0;

        for (i = 0; i < len * 16; i++) {
                i1 = i / 16;
                i2 = 15 - i % 16;
                a = !! ((buffer[i1]) & (1 << i2));
                b += sprintf(outbuf++, "%d", a);
        }
	*outbuf = '\0';
	return b;
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
