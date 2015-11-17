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

static inline int bitmap_is_superior(MYTYPE *buffer1, MYTYPE *buffer2, int len)
{
	int i, res;

	for (i = 0; i < len; i++) {
		if (buffer1[i] < buffer2[i]) {
			res = 1;
			break;
		} else if (buffer1[i] > buffer2[i]) {
			res = 0;
			break;
		}
	}
	return res;
}
#else
#endif
