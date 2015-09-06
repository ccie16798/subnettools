#ifndef ST_MEMORY_H
#define ST_MEMORY_H

extern unsigned long total_memory;

void *st_malloc_nodebug(unsigned long n, const char *s); 
void *st_malloc(unsigned long n, const char *s);

void *st_realloc_nodebug(void *ptr, unsigned long new, unsigned long old, const char *s);
void *st_realloc(void *ptr, unsigned long newn, unsigned long oldn, const char *s);

char *st_strdup(const char *s);
void st_free_string(char *s);
#else
#endif
