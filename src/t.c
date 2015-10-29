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

	b = atoi(argv[1]);
	l = atoi(argv[1]) + (unsigned long long)atoi(argv[1]) << 32;
	printf("%d %llu\n", nextPow2_32(b), nextPow2_64(l));
	res = strxcpy(buff, argv[1], sizeof(buff));
	printf("'%s' '%s' res=%d\n", argv[1], buff, res);
	printf("%d %d\n", sizeof(long), sizeof(unsigned long long));
}
