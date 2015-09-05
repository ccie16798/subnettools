#ifndef IPAM_H
#define IPAM_H

#include "st_options.h"
#include "iptools.h"
#include "st_object.h"

#ifndef SIZE_T_MAX
#define SIZE_T_MAX ((size_t)0 - 1)
#endif

#include "st_handle_csv_files.h"
struct  ipam_ea {
	char *name;
	char *value; /* value of EA; MUST be malloc'ed*/
	int len;
};

struct ipam_line {
	struct subnet subnet;
	int ea_nr; /* number of Extensible Attributes */
	struct ipam_ea *ea;
};


struct ipam_file {
	struct ipam_line *lines;
	unsigned long nr;
	unsigned long max_nr; /* the number of routes that has been malloc'ed */
	int ea_nr; /* number of Extensible Attributes */
	struct ipam_ea *ea;
};
/* return the malloc'd size of ea*/
int ea_size(struct ipam_ea *ea);

/* set value of 'ea' to 'value'
 * returns : 	-1 if no memory
 *		1  if SUCCESS
 **/
int ea_strdup(struct ipam_ea *ea, const char *value);
int alloc_ipam_file(struct ipam_file *sf, unsigned long n, int ea_nr);
void free_ipam_file(struct ipam_file *sf);
int load_ipam(char  *name, struct ipam_file *sf, struct st_options *nof);
int fprint_ipamfilter_help(FILE *out);
int ipam_file_filter(struct ipam_file *sf, char *expr);
int populate_sf_from_ipam(struct subnet_file *sf, struct ipam_file *ipam);

#else
#endif
