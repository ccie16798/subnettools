#ifndef IPAM_EA_H
#define IPAM_EA_H

#include "st_memory.h"

/* max number of EA attached to a route
 * current value seems reasonable
 */
#define MAX_EA_NUMBER 8096


struct  st_ea {
	const char *name; /* name must not be malloced or strdup; it has to point to a table of name or static string */ 
	char *value; /* value of EA; MUST be malloc'ed*/
	int len; /* strlen(value) + 1 */
};

/* return the malloc'd size of ea*/
static inline int ea_size(const struct st_ea *ea)
{
        if (ea->value == NULL)
                return 0;
        return ea->len;
}

static inline void free_ea(struct st_ea *ea) {
	if (ea->value == NULL) {
		if (ea->len) {
			fprintf(stderr, "%s: BUG freeing NULL ea value with len %d\n",
					__func__, ea->len);
			ea->len = 0;
		}
		return;
	}
	st_free(ea->value, ea->len);
	ea->len   = 0;
	ea->value = NULL;
}
/* set value of 'ea' to 'value'
 * ea->value should be freed before
 * returns:
 *	-1 if no memory
 *	1  if SUCCESS
 **/
int ea_strdup(struct st_ea *ea, const char *value);

void free_ea_array(struct st_ea *ea, int n);

/*  alloc_ea_array
 *  alloc an array of Extended Attributes
 *	@n : number of Extended Attributes
 *  returns:
 *	a pointer to a struct st_ea
 *	NULL if ENOMEM
 */
struct st_ea *alloc_ea_array(int n);

/*
 *  realloc_ea_array
 *  increase size of an EA array; set new members to NULL
 *  @ea    : old array
 *  @old_n : previous array size
 *  @new_n : new array size
 *  returns:
 *	a pointer to a new struct st_ea
 *	NULL if ENOMEM
 */
struct st_ea *realloc_ea_array(struct st_ea *ea, int old_n, int new_n);

/*
 * filter ea array to see if EA with name 'ea_name' matches 'value' using operator 'op'
 * @ea      : the EA array
 * @ea_nr   : length of  EA array
 * @ea_name : the name of the EA to filter on
 * @value   : the value to match
 * @op      : the operator (=, #, <, >, ~)
 * returns:
 *	1  if match
 *	0  if no match
 *	-1 on error
 */
int filter_ea(const struct st_ea *ea, int ea_nr, const char *ea_name,
		const char *value, char op);
#else
#endif
