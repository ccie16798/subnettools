#ifndef IPAM_EA_H
#define IPAM_EA_H

struct  ipam_ea {
	char *name;
	char *value; /* value of EA; MUST be malloc'ed*/
	int len;
};

/* return the malloc'd size of ea*/
int ea_size(struct ipam_ea *ea);
/* set value of 'ea' to 'value'
 * returns : 	-1 if no memory
 *		1  if SUCCESS
 **/
int ea_strdup(struct ipam_ea *ea, const char *value);

#else
#endif
