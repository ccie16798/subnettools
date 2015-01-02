#ifndef SUBNET_TOOLS_H
#define SUBNET_TOOLS_H

#define MAX_DELIM 32
#define PROG_NAME "subnet_tool"
#define PROG_VERS "0.7"

struct options {
        int subnet_off;
        int grep_field; /** when grepping, grep only on this field **/
        int simplify_mode; /* mode == 0 means we print the simpliefied routes, == 1 print the routes we can discard */
	char *config_file; /* config file name; default is 'st.conf' */
        char delim[MAX_DELIM];
        FILE *output_file;
	int ip_compress_mode; /* ==0 means no adress compression, 1 we remove leading 0, 2 means full compression */
	char output_fmt[128];
	/* set from config file only */
	/* IPAM FILE description */
	char ipam_prefix_field[32];
	char ipam_mask[32];
	char ipam_comment1[32];
	char ipam_comment2[32];
	char ipam_delim[MAX_DELIM];
	/* netcsv FILE description */
	char netcsv_prefix_field[32];
	char netcsv_mask[32];
	char netcsv_comment[32];
	char netcsv_device[32];
	char netcsv_gw[32];
};

int load_netcsv_file(char *name, struct subnet_file *sf, struct options *nof);
int load_PAIP(char  *name, struct subnet_file *sf, struct options *nof);
void compare_files(struct subnet_file *sf1, struct subnet_file *sf2, struct options *nof);
int missing_routes(const struct subnet_file *sf1, const struct subnet_file *sf2, struct subnet_file *sf3);
void diff_files(const struct subnet_file *sf1, const struct subnet_file *sf2, struct options *nof);
void print_file_against_paip(struct subnet_file *sf1, const struct subnet_file *paip, struct options *nof);
int network_grep_file(char *name, struct options *nof, char *ip);

int subnet_sort_ascending(struct subnet_file *sf);

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
#else
#endif
