#ifndef ST_MEMORY_H
#define ST_MEMORY_H

extern unsigned long total_memory;

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
void *st_realloc_nodebug(void *ptr, unsigned long new, unsigned long old, const char *s);
void *st_realloc(void *ptr, unsigned long newn, unsigned long oldn, const char *s);

char *st_strdup(const char *s);
void st_free_string(char *s);
void st_free(void *ptr, unsigned long len);
#else
#endif
