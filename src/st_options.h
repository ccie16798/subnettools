/* SPDX-License-Identifier: GPL-2.0 */
#ifndef ST_OPTIONS_H
#define ST_OPTIONS_H

/* debug or not parse ipv4, ipv6 low-level func */
#define DEBUG_PARSE_IPV4
#define DEBUG_PARSE_IPV6

/* use Subnet tool memory tracking for memory leak prevention
 * comment this for performance, but it shouldn't make a lot of difference
 */
#define DEBUG_ST_MEMORY

/* max size of a string collected by st_object */
#define ST_OBJECT_MAX_STRING_LEN (128 - 2)

#define MAX_DELIM 32
#define FMT_LEN	  128
#define CSV_MAX_FIELD_LENGTH 	32
#define FSCANF_LINE_LENGTH 	2048
#define MAX_COLLECTED_EA	128

#define DEFAULT_FMT "%I;%m;%D;%G;%O#"

struct st_options {
	int subnet_off;
	int print_header;
	int grep_field; /* when grepping, grep only on this field **/
	int simplify_mode; /* == 0 means we print the simplified routes,
			    * == 1 print the routes we can discard
			    */
	char *config_file; /* config file name; default is 'st.conf' */
	char delim[MAX_DELIM];
	FILE *output_file;
	int ip_compress_mode; /* IPv6 address compression */
	char output_fmt[FMT_LEN];
	char bgp_output_fmt[FMT_LEN];
	char ipam_output_fmt[FMT_LEN];
	char scanf_output_fmt[FMT_LEN];
	/* set from config file only */
	/* IPAM FILE description */
	char ipam_prefix_field[CSV_MAX_FIELD_LENGTH];
	char ipam_mask[CSV_MAX_FIELD_LENGTH];
	char ipam_comment1[CSV_MAX_FIELD_LENGTH];
	char ipam_comment2[CSV_MAX_FIELD_LENGTH];
	char ipam_delim[MAX_DELIM];
	char ipam_ea[MAX_COLLECTED_EA];
	char ipam_comment_delim;
	char ipam_comment_delim_escape;

	/* netcsv FILE description */
	char netcsv_prefix_field[CSV_MAX_FIELD_LENGTH];
	char netcsv_mask[CSV_MAX_FIELD_LENGTH];
	char netcsv_comment[CSV_MAX_FIELD_LENGTH];
	char netcsv_device[CSV_MAX_FIELD_LENGTH];
	char netcsv_gw[CSV_MAX_FIELD_LENGTH];
	/* converter options */
	int rt; /* dynamic type as a comment */
	int ecmp; /* print 2 routes in case of ecmp */
};
#else
#endif
