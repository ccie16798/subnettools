#ifndef UTILS_H
#define UTILS_H

int isInt(const char *s);
int isUnsignedInt(const char *s);
int isPower2(unsigned int x);
/*
 * string2int will transform string 's' into an 'int'
 * 'res' will be set to :
 *  0	if 's' represents an INT
 * -1	if 's' doesn't represent an int
 *  -2	if s is NULL (which shouldn't happen)
 */
int string2int(const char *s, int *res);
int mylog2(unsigned int x);

unsigned nextPow2_32(unsigned x);
unsigned long long nextPow2_64(unsigned long long x);

extern const int hex_tab[];
extern const int int_tab[];
/* take a isxdigit char as argument;
 *  returns is decimal conversion
 */
#define char2int(__c) hex_tab[(int)(__c)]

/* remove all spaces from char */
char *remove_space(char *s);

/* remove ending spaces from s
 * "etienne is a genius    " will be changed to "etienne is a genius"
 */
void remove_ending_space(char *s);

/* count the number of occurence of 'c' in string 's' */
int count_char(const char *s, char c);

/* strtok variants ; dont treat consecutive delim chars as one */
char *st_strtok(char *s, const char *delim);
char *st_strtok_r(char *s, const char *delim, char **save_ptr);
/* in case there is just one delim, use this variant */
char *st_strtok_r1(char *s, const char *delim, char **s2);

/* equivalent to strlcpy on some platforms
 * @dest  : destination string
 * @src   : source string
 * @size  : will copy size - 1 char to dst, and finally put a NUL char
 * returns:
 *	 strlen(src) so truncation occur if (return value >= size)
 */
int strxcpy(char *dest, const char *src, int size);

/* copy src into dst as long as src!= end
 * copy end + NULL into dst
 * returns:
 *	 strlen(dest) if successfull
 *	-1 if end is not found before either EOS or n - 2
 */
int strxcpy_until(char *dst, const char *src, int n, char end) __attribute__ ((warn_unused_result));

/* fgets variant; if line is longer than size, discard the exceeding char ; number of
 * discarded chars is returned in *res
 * @buffer  : buffer to store data
 * @size    : size of buffer
 * @stream  : FILE pointer
 * @res     : error; set to zero if line < size, set to number of discarded char otherwise
 * returns:
 *	pointer to buffer, NULL if EOF
 */
char *fgets_truncate_buffer(char *buffer, int size, FILE *stream, int *res);

/* macros (copied from linux kernel) */
#define max(x, y) ({ \
		typeof(x) _max1 = (x); \
		typeof(y) _max2 = (y); \
		(void) (&_max1 == &_max2); \
		_max1 > _max2 ? _max1 : _max2; })

#define min(x, y) ({ \
		typeof(x) _max1 = (x); \
		typeof(y) _max2 = (y); \
		(void) (&_max1 == &_max2); \
		_max1 < _max2 ? _max1 : _max2; })

#define min_t(type, x, y) ({ \
		type _max1 = (x); \
		type _max2 = (y); \
		_max1 < _max2 ? _max1 : _max2; })

#define max_t(type, x, y) ({ \
		type _max1 = (x); \
		type _max2 = (y); \
		_max1 > _max2 ? _max1 : _max2; })

/* v - 0x01010101 ==> bytes will have higher bit to 1 if ZERO OR > 0x80
 * ~v & 0x80808080 ==> high bit will be set only if byte < 0x80
 */
#define haszero(v) (((v) - 0x01010101UL) & ~(v) & 0x80808080UL)

/*
 * Integer to String MACROs
 * Strings ARE NOT Nul-terminated
 */
#define sprint_hex(__type) \
static inline int  sprint_hex##__type(char *s, unsigned __type a) \
{ \
	char c[32]; \
	int j, i = 0; \
\
	do { \
		if ((a & 0x0f) < 10) \
			c[i] = '0' + (a & 0x0f); \
		else \
			c[i] = 'a' + ((a & 0x0f) - 10); \
		a >>= 4; \
		i++; \
	} while (a); \
	for (j = 0; j < i; j++) \
		s[j] = c[i - j - 1]; \
	return i; \
}

#define sprint_unsigned(__type) \
static inline int sprint_u##__type(char *s, unsigned __type a) \
{ \
	char c[32]; \
	int j, i = 0; \
\
	do { \
		c[i] = '0' + a % 10; \
		a /= 10; \
		i++; \
	} while (a); \
	for (j = 0; j < i; j++) \
		s[j] = c[i - j - 1]; \
	return i; \
}

#define  sprint_signed(__type) \
static inline int sprint_##__type(char *s, __type b) \
{ \
	char c[32]; \
	int j, i = 0; \
	unsigned __type a = b; \
	int is_signed = 0; \
	if (a >  ((unsigned __type)-1) / 2) { \
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
static inline int  snprint_hex##__type(char *s, unsigned __type a, size_t __len) \
{ \
	char c[32]; \
	int j, i = 0; \
\
	do { \
		if ((a & 0xf) < 10) \
			c[i] = '0' + (a & 0xf); \
		else \
			c[i] = 'a' + ((a & 0xf) - 10); \
		a >>= 4; \
		i++; \
	} while (a); \
	for (j = 0; j < min_t(int, i, __len); j++) \
		s[j] = c[i - j - 1]; \
	return j; \
}

#define snprint_unsigned(__type) \
static inline int snprint_u##__type(char *s, unsigned __type a, size_t __len) \
{ \
	char c[32]; \
	int j, i = 0; \
\
	do { \
		c[i] = '0' + a % 10; \
		a /= 10; \
		i++; \
	} while (a); \
	for (j = 0; j < min_t(int, i, __len); j++) \
		s[j] = c[i - j - 1]; \
	return j; \
}

#define snprint_signed(__type) \
static inline int snprint_##__type(char *s, __type b, size_t __len) \
{ \
	char c[32]; \
	int j, i = 0; \
	unsigned __type a = b; \
	int is_signed = 0; \
	if (a >  ((unsigned __type)-1) / 2) { \
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
	for (j = 0; j < min(i, (int)__len - is_signed); j++) \
		s[j + is_signed] = c[i - j - 1]; \
	return (j + is_signed); \
}

#else
#endif
