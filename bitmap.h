#ifndef BITMAP_H
#define BITMAP_H

void shift_right(unsigned  short *buffer, int len, int shift);
void shift_left(unsigned  short *buffer, int len, int shift);

int sprint_bitmap(char *outbuf, unsigned  short *buffer, int len);
#else
#endif
