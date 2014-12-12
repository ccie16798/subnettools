#include <stdio.h>
#include <unistd.h>

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
	printf("%-20s;%-20s;\n", argv[1], argv[2]);
	read(0, NULL, 1);

	printf("%d %d %d \n", offsetof(struct options, delim),  offsetof(struct options, delim2),  offsetof(struct options, delim4));
}
