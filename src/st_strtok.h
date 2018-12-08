#ifndef ST_STRTOK
#define ST_STRTOK

/* strtok variants ; dont treat consecutive delim chars as one */
char *st_strtok(char *s, const char *delim);
char *st_strtok_r(char *s, const char *delim, char **save_ptr);
/* in case there is just one delim, use this variant */
char *st_strtok_r1(char *s, const char *delim, char **s2);

char *st_strtok_string_r(char *s, const char *delim, char **save_ptr,
		char comment_delim,
		char comment_delim_escape);
char *st_strtok_string_r1(char *s, const char *delim, char **save_ptr,
		char comment_delim,
		char comment_delim_escape);
#else
#endif
