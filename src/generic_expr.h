#ifndef GENERIC_ST_EXPR
#define GENERIC_ST_EXPR

struct generic_expr {
	char *pattern; /* unused */
	size_t pattern_len;
	int (*compare)(char *, char *, char);
};

int simple_expr(char *pattern, int len, struct generic_expr *e);
int run_generic_expr(char *pattern, int len, struct generic_expr *e);
int int_compare(char *, char *, char);

#else
#endif
