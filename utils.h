#ifndef UTILS_H
#define UTILS_H

int isInt(const char *s);
int isUnsignedInt(const char *s);
int isPower2(unsigned int x);
int mylog2(unsigned int x);
/* take a isxdigit char as argument;
   returns is decimal conversion
*/
int char2int(char c);

/* remove all spaces from char */
char *remove_space(char *s);

/* strtok variants ; dont treat consecutive delim chars as one */
char *simple_strtok(char *s, const char *delim);
char *simple_strtok_r(char *s, const char *delim, char **save_ptr) ;

/* equivalent to strlcpy on some platforms */
int strxcpy(char *dest, const char *src, int size);

/* copy src into dst as long as src!= end
 * copy end + EOS into dst
 * return strlen(dest) if successfull
 * return -1 if end is not found before either EOS or n - 2
 */
int strxcpy_until(char *dst, const char *src, int n, char end)  __attribute__ ((warn_unused_result));

/* fgets variant; if line is longer than size, discard the exceeding char ; number of
 * discarded chars is returned in *res
 */
char *fgets_truncate_buffer(char *buffer, int size, FILE *stream, int *res);

/* macros (copied from linux kernel) */
#define max(x, y) ({                            \
        typeof(x) _max1 = (x);                  \
        typeof(y) _max2 = (y);                  \
        (void) (&_max1 == &_max2);              \
        _max1 > _max2 ? _max1 : _max2; })


#define min(x, y) ({                            \
        typeof(x) _max1 = (x);                  \
        typeof(y) _max2 = (y);                  \
        (void) (&_max1 == &_max2);              \
        _max1 < _max2 ? _max1 : _max2; })


/*
 * Integer to String MACROs
 * Strings ARE NOT Nul-terminated
 */
#define sprint_hex(__type) \
inline int  sprint_hex##__type(char *s, unsigned __type a) { \
        char c[32]; \
        int j, i = 0; \
\
        do { \
		if (a % 16 < 10) \
                	c[i] = '0' + a % 16; \
		else \
			c[i] = 'a' + (a - 10) % 16; \
                a /= 16; \
                i++; \
        } while (a); \
        for (j = 0; j < i;j++) \
                s[j] = c[i - j - 1]; \
        return i; \
}

#define sprint_unsigned(__type) \
static inline int sprint_u##__type(char *s, unsigned __type a) { \
	char c[32]; \
	int j, i = 0; \
\
	do { \
		c[i] = '0' + a % 10; \
		a /= 10; \
		i++; \
	} while (a); \
	for (j = 0; j < i;j++) \
		s[j] = c[i - j - 1]; \
	return i; \
}

#define  sprint_signed(__type) \
static inline int sprint_##__type (char *s, __type b) { \
	char c[32]; \
	int j, i = 0; \
	unsigned __type a = b; \
	int is_signed = 0; \
	if ( a >  ((unsigned __type)-1) / 2) { \
		s[0] = '-'; \
		a = ((unsigned __type)-1) - a; \
		a++; \
		is_signed = 1; \
	} \
\
	do { \
		c[i] = '0' + a % 10; \
		a /= 10; \
		i++; \
       } while (a); \
       for (j = 0; j < i; j++) \
               s[j + is_signed] = c[i - j - 1]; \
       return (i + is_signed); \
}

#define snprint_hex(__type) \
inline int  snprint_hex##__type(char *s, unsigned __type a, size_t __len) { \
        char c[32]; \
        int j, i = 0; \
\
        do { \
		if (a % 16 < 10) \
                	c[i] = '0' + a % 16; \
		else \
			c[i] = 'a' + (a - 10) % 16; \
                a /= 16; \
                i++; \
        } while (a); \
        for (j = 0; j < min(i, __len);j++) \
                s[j] = c[i - j - 1]; \
        return j; \
}

#define snprint_unsigned(__type) \
static inline int snprint_u##__type(char *s, unsigned __type a, size_t __len) { \
	char c[32]; \
	int j, i = 0; \
\
	do { \
		c[i] = '0' + a % 10; \
		a /= 10; \
		i++; \
	} while (a); \
	for (j = 0; j < min(i, __len);j++) \
		s[j] = c[i - j - 1]; \
	return j; \
}

#define snprint_signed(__type) \
static inline int snprint_##__type (char *s, __type b, size_t __len) { \
	char c[32]; \
	int j, i = 0; \
	unsigned __type a = b; \
	int is_signed = 0; \
	if ( a >  ((unsigned __type)-1) / 2) { \
		s[0] = '-'; \
		a = ((unsigned __type)-1) - a; \
		a++; \
		is_signed = 1; \
	} \
\
	do { \
		c[i] = '0' + a % 10; \
		a /= 10; \
		i++; \
	} while (a); \
	for (j = 0; j < min(i, __len) - is_signed; j++) \
	s[j + is_signed] = c[i - j - 1]; \
	return (j + is_signed); \
}

#else
#endif
