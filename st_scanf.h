#ifndef ST_SCANF_H
#define ST_SCANF_H


int st_sscanf(char *input, const char *fmt, ...);
int st_fscanf(FILE *f, const char *fmt, ...);

/* subnet tools object */
struct sto { 
	char type;
	union {
		struct subnet 	s_subnet;
		int 		s_int;
		long 		s_long;
		char		s_char[64];
	};
};

#else
#endif
