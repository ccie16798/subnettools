#ifndef ST_HANDLE_CSV_FILES_H
#define ST_HANDLE_CSV_FILES_H

#include "st_options.h"
int load_netcsv_file(char *name, struct subnet_file *sf, struct st_options *nof);
int load_PAIP(char  *name, struct subnet_file *sf, struct st_options *nof);

#else
#endif
