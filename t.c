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
int main(int argc, char **argv) {
	char buff[32];
	struct subnet s;
	short a;
	int b = -1234;
	int res;

	memset(buff, 0, 28);
	sscanf(argv[1], "%x", &a);
	res = snprint_hexshort(buff, a, 12);
	buff[res] = '\0';
	printf("%s\n", buff);
	res = snprint_int(buff, b, 3);
	buff[res] = '\0';
	printf("%s\n", buff);

//	printf("%d %d %d \n", offsetof(struct options, delim),  offsetof(struct options, delim2),  offsetof(struct options, delim4));
	printf("%d %d\n", sizeof(long), sizeof(unsigned long long));
}
