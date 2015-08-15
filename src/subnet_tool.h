#ifndef SUBNET_TOOLS_H
#define SUBNET_TOOLS_H

#include "st_options.h"
int load_netcsv_file(char *name, struct subnet_file *sf, struct st_options *nof);
int load_PAIP(char  *name, struct subnet_file *sf, struct st_options *nof);
void compare_files(struct subnet_file *sf1, struct subnet_file *sf2, struct st_options *nof);
int missing_routes(const struct subnet_file *sf1, const struct subnet_file *sf2, struct subnet_file *sf3);
void diff_files(const struct subnet_file *sf1, const struct subnet_file *sf2, struct st_options *nof);
void print_file_against_paip(struct subnet_file *sf1, const struct subnet_file *paip, struct st_options *nof);
int network_grep_file(char *name, struct st_options *nof, char *ip);

int subnet_sort_ascending(struct subnet_file *sf);
int subnet_sort_by(struct subnet_file *sf, char *name);
void subnet_available_cmpfunc(FILE *out);
int fprint_routefilter_help(FILE *out);
int subnet_filter(struct subnet_file *sf, char *expr);
/* remove duplicate/included entries, and sort */
int subnet_file_simplify(struct subnet_file *sf);
/* same but take GW into account, must be equal 
 if mode == 0, prints the simplified route file
 if mode == 1, prints the routes that can be removed 
*/
int route_file_simplify(struct subnet_file *sf,  int mode);
/* aggregates entries from 'sf' as much as possible
 * mode == 1 means we take the GW into acoount
 * mode == 0 means we dont take the GW into account
 */
int aggregate_route_file(struct subnet_file *sf, int mode);
int subnet_file_merge_common_routes(const struct subnet_file *sf1,  const struct subnet_file *sf2, struct subnet_file *sf3);
unsigned long long sum_subnet_file(struct subnet_file *sf);

int subnet_file_remove(const struct subnet_file *sf1, struct subnet_file *sf2, const struct subnet *s2);

/* split s, "n,m,k" means :
 *   first split 's' n times,
 *   second splits resulting subnet m times
 *   last splits resulting subnet k times
 *   split will produce 'n * m * k' subnets */
int subnet_split(FILE *out, const struct subnet *s, char *string_levels);
/* split2 s, "n,m,k" means :
 *   first split 's' into /n mask,
 *   second splits resulting subnet in /m masks
 *   etc...
 */
int subnet_split_2(FILE *out, const struct subnet *s, char *string_levels);
#else
#endif
