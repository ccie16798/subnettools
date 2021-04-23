#ifndef ST_MEMORY_H
#define ST_MEMORY_H

extern unsigned long total_memory;
#include <stdlib.h>
#include "st_options.h"

/* this option is set in st_options.h */
#ifdef DEBUG_ST_MEMORY

/* st_malloc, st_realloc will print debug MSG if debug memory level > 3
 * this is unsuitable for small allocations
 */
void *__st_malloc_nodebug(unsigned long n, const char *desc,
		const char *file, const char *func, int line);
void *__st_malloc(unsigned long n, const char *desc,
		const char *file, const char *func, int line);
#define st_malloc(__n, __desc) \
	__st_malloc(__n, __desc, __FILE__, __func__, __LINE__)

#define st_malloc_nodebug(__n, __desc) \
	__st_malloc_nodebug(__n, __desc, __FILE__, __func__, __LINE__)
void *__st_realloc_nodebug(void *ptr, unsigned long new, unsigned long old, const char *desc,
		const char *file, const char *func, int line);
void *__st_realloc(void *ptr, unsigned long newn, unsigned long oldn, const char *desc,
		const char *file, const char *func, int line);
#define st_realloc_nodebug(__ptr, __new, __old, __s) \
	__st_realloc_nodebug(__ptr, __new, __old, __s, __FILE__, __func__, __LINE__)

#define st_realloc(__ptr, __new, __old, __desc) \
	__st_realloc(__ptr, __new, __old, __desc, __FILE__, __func__, __LINE__)
char *__st_strdup(const char *s,
		const char *file, const char *func, int line);
#define st_strdup(__s) \
		__st_strdup(__s, __FILE__, __func__, __LINE__)
void *__st_memdup(const char *s, size_t len,
		const char *file, const char *func, int line);
#define st_memdup(__s, __len) \
		__st_memdup(__s, __len, __FILE__, __func__, __LINE__)
void st_free_string(char *s);
void st_free(void *ptr, unsigned long len);

#else

void st_simple_memdup(const void *s, size_t len);
#define st_malloc(__n, __desc) malloc(__n)
#define st_malloc_nodebug(__n, __desc) malloc(__n)
#define st_realloc_nodebug(__ptr, __new, __old, __desc) realloc(__ptr, __new)
#define st_realloc(__ptr, __new, __old, __desc) realloc(__ptr, __new)
#define st_free(__ptr, __n) free(__ptr)
#define st_free_string(__s) free(__s)
#define st_strdup(__s) strdup(__s)
#define st_memdup(__s, __len)  st_simple_memdup(__s, __len)

#endif

#else
#endif
