#ifndef BITMAP_H
#define BITMAP_H

#define MYTYPE    unsigned short
#define TYPE_SIZE (sizeof(MYTYPE) * 8)
#define TYPE_MAX  ((MYTYPE)-1)

void shift_right(MYTYPE *buffer, int len, int shift);
void shift_left(MYTYPE *buffer, int len, int shift);

void decrease_bitmap(MYTYPE *buffer, int len);
void increase_bitmap(MYTYPE *buffer, int len);

int sprint_bitmap(char *outbuf, unsigned  short *buffer, int len);
#else
#endif
