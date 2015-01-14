#ifndef ST_SCANF_H
#define ST_SCANF_H
#include "st_object.h"

int st_fscanf(FILE *f, const char *fmt, ...);
int st_sscanf(char *in, const char *fmt, ...);
int sto_sscanf(char *in, const char *fmt, struct sto *o, int max_o);

#else
#endif
