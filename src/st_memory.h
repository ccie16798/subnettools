#ifndef ST_MEMORY_H
#define ST_MEMORY_H

extern unsigned long total_memory;
#include "st_options.h"

/* this option is set in st_options.h */
#ifdef DEBUG_ST_MEMORY

/* st_malloc, st_realloc will print debug MSG if debug memory level > 3
 * this is unsuitable for small allocations
 */
void *__st_malloc_nodebug(unsigned long n, const char *s,
		const char *file, const char *func, int line);
void *__st_malloc(unsigned long n, const char *s,
		const char *file, const char *func, int line);
#define st_malloc(__n, __s) \
	__st_malloc(__n, __s, __FILE__, __func__, __LINE__)

#define st_malloc_nodebug(__n, __s) \
	__st_malloc_nodebug(__n, __s, __FILE__, __func__, __LINE__)
void *__st_realloc_nodebug(void *ptr, unsigned long new, unsigned long old, const char *s,
		const char *file, const char *func, int line);
void *__st_realloc(void *ptr, unsigned long newn, unsigned long oldn, const char *s,
		const char *file, const char *func, int line);
#define st_realloc_nodebug(__ptr, __new, __old, __s) \
	__st_realloc_nodebug(__ptr, __new, __old, __s, __FILE__, __func__, __LINE__)

#define st_realloc(__ptr, __new, __old, __s) \
	__st_realloc(__ptr, __new, __old, __s, __FILE__, __func__, __LINE__)
char *__st_strdup(const char *s,
		const char *file, const char *func, int line);
#define st_strdup(__s) \
		__st_strdup(__s, __FILE__, __func__, __LINE__)

#define st_memdup(__s, __len) \
		__st_memdup(__s, __len, __FILE__, __func__, __LINE__)
void st_free_string(char *s);
void st_free(void *ptr, unsigned long len);

#else

#define st_malloc(__n, __s) malloc(__n)
#define st_malloc_nodebug(__n, __s) malloc(__n)
#define st_realloc_nodebug(__ptr, __new, __old, __s) realloc(__ptr, __new)
#define st_realloc(__ptr, __new, __old, __s) realloc(__ptr, __new)
#define st_free(__ptr, __n) free(__ptr)
#define st_free_string(__s) free(__s)
#define st_strdup(__s) strdup(__s)
void st_memdup(const void *s, size_t len) {
	char *b;

	if (s == NULL)
		return NULL;
	b = malloc(len);
	if (b == NULL)
		return NULL;
	memcpy(b, s, len);
	return b;
}

#endif

#else
#endif
