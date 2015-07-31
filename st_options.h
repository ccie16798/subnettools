#ifndef ST_OPTIONS_H
#define ST_OPTIONS_H


#define MAX_DELIM 32
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
#else
#endif
