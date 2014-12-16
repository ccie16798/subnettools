#include <stdio.h>
#include <unistd.h>
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

int mask2str(unsigned int mask, char *out) {
	int a = mask / 8;
	int b = mask % 8;
	int i;
	int s[4];
	for (i = 0; i < a; i++)
		s[i] = 255;
	s[i++] = 256 - (1U << (8 - b));
	for ( ; i < 4; i++)
		s[i] = 0; 
        sprintf(out, "%d.%d.%d.%d", s[0], s[1], s[2], s[3]);
        return 0;
}

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
int main(int argc, char **argv) {
	char buff[40];
	printf("%-20s;%-20s;\n", argv[1], argv[2]);
	mask2str(atoi(argv[1]), buff);
	printf("%d : %s\n", atoi(argv[1]), buff);
//	printf("%d %d %d \n", offsetof(struct options, delim),  offsetof(struct options, delim2),  offsetof(struct options, delim4));
	printf("%d %d\n", sizeof(long), sizeof(unsigned long long));
}
