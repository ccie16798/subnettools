#ifndef GENERIC_ST_EXPR
#define GENERIC_ST_EXPR

struct generic_expr {
	char *pattern;
	size_t pattern_len;
	int (*compare)(char *, char *, char);
};



#else
#endif
