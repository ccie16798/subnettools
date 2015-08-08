#ifndef GENERIC_ST_EXPR
#define GENERIC_ST_EXPR

struct generic_expr {
	const char *pattern;
	size_t pattern_len;
	int recursion_level;
	int (*compare)(char *, char *, char);
};

#define GENERIC_ST_MAX_RECURSION 10

void init_generic_expr(struct generic_expr *e, const char *s, int (*compare)(char *, char *, char));
int simple_expr(char *pattern, int len, struct generic_expr *e);
int run_generic_expr(char *pattern, int len, struct generic_expr *e);
int int_compare(char *, char *, char);

#else
#endif
