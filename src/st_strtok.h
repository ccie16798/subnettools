#ifndef ST_STRTOK
#define ST_STRTOK

/* strtok variants ; don't treat consecutive delim chars as one
 * @s        : input string; if NULL, use the previous string result
 * @save_ptr : previous string for consecutive calls
 * @delim: a list of delim char that separate tokens
 * returns:
 *     NULL if no more token
 *     a pointer to a token
 * NOTE:
 *     for re-entrant variants, the previous token is stored in
 *     save_ptr,
 *     non re-entrant use a static char * (so usually unsafe to use)
 * */
 
char *st_strtok(char *s, const char *delim);
char *st_strtok_r(char *s, const char *delim, char **save_ptr);
/* in case there is just one delim, use this variant */
char *st_strtok_r1(char *s, const char *delim, char **s2);

char *st_strtok_string_r(char *s, const char *delim, char **save_ptr,
		char string_delim,
		char string_delim_escape);
char *st_strtok_string_r1(char *s, const char *delim, char **save_ptr,
		char string_delim,
		char string_delim_escape);
#else
#endif
