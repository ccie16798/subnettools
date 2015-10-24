#ifndef ST_MEMORY_H
#define ST_MEMORY_H

extern unsigned long total_memory;

/* st_malloc, st_realloc will print debug MSG if debug memory level > 3
 * this is unsuitable for small allocations
 */
void *st_malloc_nodebug(unsigned long n, const char *s); 
void *st_malloc(unsigned long n, const char *s);

void *st_realloc_nodebug(void *ptr, unsigned long new, unsigned long old, const char *s);
void *st_realloc(void *ptr, unsigned long newn, unsigned long oldn, const char *s);

char *st_strdup(const char *s);
void st_free_string(char *s);
void st_free(void *ptr, unsigned long len);
#else
#endif
