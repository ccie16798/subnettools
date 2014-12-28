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
char *remove_space(char *s) {
        int i = 0, j = 0;

        while (1) {
                if (isspace(s[i])) {
                        i++;
                        continue;
                }
                if (i == j) {
                        i++;
                        j++;
			if (s[i] == '\0')
				return s;
                        continue;
                }
                s[j] = s[i];
                if (s[i] == '\0')
                        return s;
		i++;
		j++;
        }
}

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
int main(int argc, char **argv) {
	char buff[40];

	printf("'%s' '%s'\n", argv[1], remove_space(argv[1]));
//	printf("%d %d %d \n", offsetof(struct options, delim),  offsetof(struct options, delim2),  offsetof(struct options, delim4));
	printf("%d %d\n", sizeof(long), sizeof(unsigned long long));
}
