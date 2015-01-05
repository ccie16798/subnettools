#include <stdio.h>
#include <unistd.h>
#include "iptools.h"
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


#define sprint_hex(__type) \
inline int  sprint_hex##__type(char *s, unsigned __type a) { \
        char c[32]; \
        int j, i = 0; \
\
        do { \
		if (a % 16 < 10) \
                	c[i] = '0' + a % 16; \
		else \
			c[i] = 'a' + (a - 10) % 16; \
                a /= 16; \
                i++; \
        } while (a); \
        for (j = 0; j < i;j++) \
                s[j] = c[i - j - 1]; \
        return i; \
}

#define sprint_unsigned(__type) \
inline int  sprint_u##__type(char *s, unsigned __type a) { \
	char c[32]; \
	int j, i = 0; \
\
	do { \
		c[i] = '0' + a % 10; \
		a /= 10; \
		i++; \
	} while (a); \
	for (j = 0; j < i;j++) \
		s[j] = c[i - j - 1]; \
	return i; \
}

#define  sprint_signed(__type) \
inline int sprint_##__type (char *s, __type b) { \
	char c[32]; \
	int j, i = 0; \
	unsigned __type a = b; \
	if ( a >  ((unsigned __type)-1) / 2) { \
		s[0] = '-'; \
		a = ((unsigned __type)-1) - a; \
		a++; \
		s++; \
	} \
\
	do { \
		c[i] = '0' + a % 10; \
		a /= 10; \
		i++; \
	} while (a); \
	for (j = 0; j < i; j++) \
		s[j ] = c[i -j - 1]; \
	return i; \
}
sprint_signed(short)
sprint_hex(short)
sprint_signed(int)
sprint_unsigned(short)
sprint_unsigned(int)
sprint_unsigned(long)

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
int main(int argc, char **argv) {
	char buff[32];
	struct subnet s;
	short a;
	memset(buff, 0, 28);
	 sscanf(argv[1], "%x", &a);
	sprint_hexshort(buff, a);
	printf("%s %hd\n", buff, a);

//	printf("%d %d %d \n", offsetof(struct options, delim),  offsetof(struct options, delim2),  offsetof(struct options, delim4));
	printf("%d %d\n", sizeof(long), sizeof(unsigned long long));
}
