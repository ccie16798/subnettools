#ifndef UTILS_H
#define UTILS_H

int isInt(const char *s);
int isUnsignedInt(const char *s);
int isPower2 (unsigned int x); 
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
char *strxcpy(char *dest, const char *src, int size);

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

#else
#endif
