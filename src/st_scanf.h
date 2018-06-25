#ifndef ST_SCANF_H
#define ST_SCANF_H
#include "st_object.h"

/* max number of objects collected by sto_vscanf */
#define ST_VSCANF_MAX_OBJECTS  40
/* max number of objects collected in an expression */
#define MAX_COLLECTED_OBJECTS 10

int st_fscanf(FILE *f, const char *fmt, ...);
int st_sscanf(const char *in, const char *fmt, ...);
int sto_sscanf(const char *in, const char *fmt, struct sto *o, int max_o);

/* case insensitive variants */
int st_fscanf_ci(FILE *f, const char *fmt, ...);
int st_sscanf_ci(const char *in, const char *fmt, ...);
int sto_sscanf_ci(const char *in, const char *fmt, struct sto *o, int max_o);
#else
#endif
