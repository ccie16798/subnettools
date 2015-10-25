#ifndef ST_MEMORY_H
#define ST_MEMORY_H

extern unsigned long total_memory;

#ifdef DEBUG_ST_MEMORY

/* st_malloc, st_realloc will print debug MSG if debug memory level > 3
 * this is unsuitable for small allocations
 */
void *__st_malloc_nodebug(unsigned long n, const char *s,
		const char *file, const char *func, int line);
void *__st_malloc(unsigned long n, const char *s,
		const char *file, const char *func, int line);
#define st_malloc(__n, __s) \
	__st_malloc(__n, __s, __FILE__, __FUNCTION__, __LINE__)

#define st_malloc_nodebug(__n, __s) \
	__st_malloc_nodebug(__n, __s, __FILE__, __FUNCTION__, __LINE__)
void *__st_realloc_nodebug(void *ptr, unsigned long new, unsigned long old, const char *s,
		const char *file, const char *func, int line);
void *__st_realloc(void *ptr, unsigned long newn, unsigned long oldn, const char *s,
		const char *file, const char *func, int line);
#define st_realloc_nodebug(__ptr, __new, __old, __s) \
	__st_realloc_nodebug(__ptr, __new, __old, __s, __FILE__, __FUNCTION__, __LINE__)

#define st_realloc(__ptr, __new, __old, __s) \
	__st_realloc(__ptr, __new, __old, __s, __FILE__, __FUNCTION__, __LINE__)
char *__st_strdup(const char *s,
		const char *file, const char *func, int line);
#define st_strdup(__s) \
		__st_strdup(__s, __FILE__, __FUNCTION__, __LINE__)
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

#endif

#else
#endif
