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


#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
int main(int argc, char **argv) {
	char buff[8];
	struct subnet s;
	int a;
	get_subnet_or_ip(argv[1], &s);
	subnet2str(&s, buff, sizeof(buff), 3);
	printf("%s\n", buff); 

//	printf("%d %d %d \n", offsetof(struct options, delim),  offsetof(struct options, delim2),  offsetof(struct options, delim4));
	printf("%d %d\n", sizeof(long), sizeof(unsigned long long));
}
