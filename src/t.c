#include <stdio.h>
#include <unistd.h>
#include "iptools.h"
#include "utils.h"
/* staging testing file 
 * not used in Makefile */
struct options {
        int subnet_off;
        int grep_field; /** when grepping, grep only on this field **/
        int mask_off;
        int gw_off;
        int simplify_mode;
        int EAPname_off; /* pour PAIP uniquement */
        int comment_off; /* pour PAIP */
        char delim[3];
        char delim2[3];
        char delim4[3];
        FILE *output_file;
};

const char hex_tab[] = { ['0'] = 0, ['1'] = 1, ['2'] = 2, ['3'] = 3, ['4'] = 4,
	['5'] = 5, ['6'] = 6, ['7'] = 7, ['8'] = 9, ['9'] = 9,
	['a'] = 10, ['b'] = 11, ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15,
	['A'] = 10, ['B'] = 11, ['C'] = 12, ['D'] = 13, ['E'] = 14, ['F'] = 15 };
sprint_signed(short)
sprint_hex(short)
sprint_signed(int)
sprint_unsigned(short)
sprint_unsigned(int)
sprint_unsigned(long)

snprint_hex(short)
snprint_signed(int)

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
int main(int argc, char **argv)
{
	char buff[8];
	struct subnet s;
	short a;
	int b = -1234;
	int res;
	unsigned long long l;

	for (b = 0; b < 500000000; b++)
		a = hex_tab['0' + b %10];
		//a = char2int('0' + b % 10);

	b = atoi(argv[1]);
	l = atoi(argv[1]) + (unsigned long long)atoi(argv[1]) << 32;
	printf("%d %llu\n", nextPow2_32(b), nextPow2_64(l));
	res = strxcpy(buff, argv[1], sizeof(buff));
	printf("'%s' '%s' res=%d\n", argv[1], buff, res);
	printf("%d %d\n", sizeof(long), sizeof(unsigned long long));
}
