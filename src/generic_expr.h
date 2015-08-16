#ifndef GENERIC_ST_EXPR
#define GENERIC_ST_EXPR

struct generic_expr {
	const char *pattern;
	size_t pattern_len;
	int recursion_level;
	void *object;
	/*
	 * ->compare will compare 'string' (interpreted will the help of 'object') against
	 *  'value', using operator 'op'
	 */
	int (*compare)(char *string, char *value, char operator, void *object);
};



#define GENERIC_ST_MAX_RECURSION 10

void init_generic_expr(struct generic_expr *e, const char *s, int (*compare)(char *, char *, char, void *));
int run_generic_expr(char *pattern, int len, struct generic_expr *e);
int int_compare(char *, char *, char, void *);

#else
#endif
