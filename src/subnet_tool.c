/*
 * Subnet_tool ! : a tool that will help network engineers :
 *
 *  - finding IPs conflicts in 2 files
 *  - extracting a subnet definition from a IPAM (IP Address Manager)
 *  - converting various 'sh ip route' formats to csv
 *  - grepping routes in a file
 *  - aggregating routes
 *
 * Copyright (C) 2014,2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "debug.h"
#include "iptools.h"
#include "utils.h"
#include "generic_csv.h"
#include "heap.h"
#include "subnet_tool.h"
#include "st_printf.h"

#define SIZE_T_MAX ((size_t)0 - 1)
int alloc_subnet_file(struct subnet_file *sf, unsigned long n) {
	if (n > SIZE_T_MAX / sizeof(struct route)) { /* being paranoid */
		fprintf(stderr, "error: too much memory requested for struct route\n");
		return -1;
	}
	sf->routes = malloc(sizeof(struct route) * n);
	debug(MEMORY, 2, "trying to alloc %lu bytes\n",  sizeof(struct route) * n);
	if (sf->routes == NULL) {
		fprintf(stderr, "error: cannot alloc  memory for sf->routes\n");
		return -1;
	}
	sf->nr = 0;
	sf->max_nr = n;
	return 0;
}

static int netcsv_prefix_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;
	int res;
	struct subnet subnet;

	res = get_subnet_or_ip(s, &subnet);
	if (res < 0) {
		debug(LOAD_CSV, 3, "invalid IP %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	copy_subnet(&sf->routes[sf->nr].subnet,  &subnet);
	return CSV_VALID_FIELD;
}

static int netcsv_mask_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;
	u32 mask = string2mask(s, 21);

	if (mask == BAD_MASK) {
		debug(LOAD_CSV, 3, "invalid mask %s line %lu\n", s, state->line);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].subnet.mask = mask;
	return CSV_VALID_FIELD;
}

static int netcsv_device_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;

	strxcpy(sf->routes[sf->nr].device, s, sizeof(sf->routes[sf->nr].device));
	if (strlen(s) >= sizeof(sf->routes[sf->nr].device))
		debug(LOAD_CSV, 1, "line %lu STRING device '%s'  too long, truncating to '%s'\n", state->line, s, sf->routes[sf->nr].device);
	return CSV_VALID_FIELD;
}

static int netcsv_GW_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;
	struct ip_addr addr;
	int res;

	res = string2addr(s, &addr, 41);
	if (res != IPV4_A && res != IPV6_A) {  /* we accept that there's no gateway but we treat it has a comment instead */
		strxcpy(sf->routes[sf->nr].comment, s, sizeof(sf->routes[sf->nr].comment));
		if (strlen(s) >= sizeof(sf->routes[sf->nr].comment))
			debug(LOAD_CSV, 1, "line %lu STRING comment '%s'  too long, truncating to '%s'\n", state->line, s, sf->routes[sf->nr].comment);
	} else {
		if (res == sf->routes[sf->nr].subnet.ip_ver) {/* does the gw have same IPversion*/
			copy_ipaddr(&sf->routes[sf->nr].gw, &addr);
		} else {
			memset(&sf->routes[sf->nr].gw, 0, sizeof(struct ip_addr));
			debug(LOAD_CSV, 3, "invalid GW %s line %lu\n", s, state->line);
		}
	}
	return CSV_VALID_FIELD;
}

static int netcsv_comment_handle(char *s, void *data, struct csv_state *state) {
	struct subnet_file *sf = data;

	strxcpy(sf->routes[sf->nr].comment, s, sizeof(sf->routes[sf->nr].comment));
	if (strlen(s) >= sizeof(sf->routes[sf->nr].comment))
		debug(LOAD_CSV, 1, "line %lu STRING comment '%s'  too long, truncating to '%s'\n", state->line, s, sf->routes[sf->nr].comment);
	return CSV_VALID_FIELD;
}

static int netcsv_is_header(char *s) {
	if (isalpha(s[0]))
		return 1;
	else
		return 0;
}

static int netcsv_endofline_callback(struct csv_state *state, void *data) {
	struct subnet_file *sf = data;
	struct route *new_r;

	if (state->badline) {
		debug(LOAD_CSV, 3, "Invalid line %lu\n", state->line);
		return -1;
	}
	sf->nr++;
	if  (sf->nr == sf->max_nr) {
		sf->max_nr *= 2;
		debug(MEMORY, 2, "need to reallocate %lu bytes\n", sf->max_nr * sizeof(struct route));
		if (sf->max_nr > SIZE_T_MAX / sizeof(struct route)) {
			debug(MEMORY, 1, "cannot allocate %llu bytes for struct route, too big\n", (unsigned long long)sf->max_nr * sizeof(struct route));
			return CSV_CATASTROPHIC_FAILURE;
		}
		new_r = realloc(sf->routes,  sizeof(struct route) * sf->max_nr);
		if (new_r == NULL) {
			fprintf(stderr, "unable to reallocate, need to abort\n");
			return  CSV_CATASTROPHIC_FAILURE;
		}
		sf->routes = new_r;
	}
	memset(&sf->routes[sf->nr], 0, sizeof(struct route));
	return CSV_CONTINUE;
}

static int netcsv_validate_header(struct csv_field *field) {
	int i;
	/* ENHANCE ME
	 * we could check if ( (prefix AND mask) OR (prefix/mask) )
	 */
	for (i = 0; ; i++) {
		if (field[i].name == NULL)
			break;
	}
	return 1;
}

int load_netcsv_file(char *name, struct subnet_file *sf, struct st_options *nof) {
	/* default netcsv fields */
	struct csv_field csv_field[] = {
		{ "prefix"	, 0, 1, 1, &netcsv_prefix_handle },
		{ "mask"	, 0, 2, 0, &netcsv_mask_handle },
		{ "device"	, 0, 0, 0, &netcsv_device_handle },
		{ "GW"		, 0, 0, 0, &netcsv_GW_handle },
		{ "comment"	, 0, 3, 0, &netcsv_comment_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;

	/* netcsv field may have been set by conf file */
	if (nof->netcsv_prefix_field[0])
		csv_field[0].name = nof->netcsv_prefix_field;
	if (nof->netcsv_mask[0])
		csv_field[1].name = nof->netcsv_mask;
	if (nof->netcsv_device[0])
		csv_field[2].name = nof->netcsv_device;
	if (nof->netcsv_gw[0])
		csv_field[3].name = nof->netcsv_gw;
	if (nof->netcsv_comment[0])
		csv_field[4].name = nof->netcsv_comment;

	init_csv_file(&cf, csv_field, nof->delim, &strtok_r);
	cf.is_header = &netcsv_is_header;
	cf.endofline_callback = &netcsv_endofline_callback;
	cf.validate_header = &netcsv_validate_header;

	if (alloc_subnet_file(sf, 4096) < 0)
		return -2;
	memset(&sf->routes[0], 0, sizeof(struct route));
	return generic_load_csv(name, &cf, &state, sf);
}

static int ipam_comment_handle(char *s, void *data, struct csv_state *state) {
        struct  subnet_file *sf = data;

	if (strlen(s) > 2) /* sometimes comment are fucked and a better one is in EA-Name */
		strxcpy(sf->routes[sf->nr].comment, s, sizeof(sf->routes[sf->nr].comment));
	if (strlen(s) >= sizeof(sf->routes[sf->nr].comment))
                debug(LOAD_CSV, 1, "line %lu STRING comment '%s'  too long, truncating to '%s'\n", state->line, s, sf->routes[sf->nr].comment);
	return CSV_VALID_FIELD;
}

int load_PAIP(char  *name, struct subnet_file *sf, struct st_options *nof) {
	/*
	 * default IPAM fields (Infoblox)
  	 * obviously if you have a different IPAM please describe it in the config file
	 */
	struct csv_field csv_field[] = {
		{ "address*"	, 0,  3, 1, &netcsv_prefix_handle },
		{ "netmask_dec"	, 0,  4, 1, &netcsv_mask_handle },
		{ "EA-Name"	, 0, 16, 1, &netcsv_comment_handle },
		{ "comment"	, 0, 17, 1, &ipam_comment_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;

	if (nof->ipam_prefix_field[0])
		csv_field[0].name = nof->ipam_prefix_field;
	if (nof->ipam_mask[0])
		csv_field[1].name = nof->ipam_mask;
	if (nof->ipam_comment1[0]) {
		csv_field[2].name = nof->ipam_comment1;
		/* if comment1 is set, we set also comment2, even if its NULL
		 * if it is NULL, that means the ipam we have doesnt have a secondary comment field
		 */
		csv_field[3].name = nof->ipam_comment2;
	}
	init_csv_file(&cf, csv_field, nof->ipam_delim, &simple_strtok_r);
        cf.is_header = netcsv_is_header;
	cf.endofline_callback = netcsv_endofline_callback;

	if (alloc_subnet_file(sf, 16192) < 0)
		return -2;
	memset(&sf->routes[0], 0, sizeof(struct route));
	return generic_load_csv(name, &cf, &state, sf);
}

void compare_files(struct subnet_file *sf1, struct subnet_file *sf2, struct st_options *nof) {
	unsigned long i, j;
	int res;
	int find = 0;

	for (i = 0; i < sf1->nr; i++) {
		find = 0;
		for (j = 0; j < sf2->nr; j++) {
			res = subnet_compare(&sf1->routes[i].subnet, &sf2->routes[j].subnet);
			if (res == INCLUDES) {
				st_fprintf(nof->output_file, "%I;%m;INCLUDES;%I;%m\n", sf1->routes[i].subnet, sf1->routes[i].subnet,
						sf2->routes[j].subnet,  sf2->routes[j].subnet);
				find = 1;
			} else if (res == EQUALS) {
				st_fprintf(nof->output_file, "%I;%m;EQUALS;%I;%m\n", sf1->routes[i].subnet, sf1->routes[i].subnet,
						sf2->routes[j].subnet,  sf2->routes[j].subnet);
				find = 1;
			}
		}
		if (find == 0)
			st_fprintf(nof->output_file, "%I;%m;;;\n", sf1->routes[i].subnet, sf1->routes[i].subnet);
	}
}

/*
 * get routes from sf1 not covered by sf2 INTO sf3 */
int missing_routes(const struct subnet_file *sf1, const struct subnet_file *sf2, struct subnet_file *sf3) {
	unsigned long i, j, k;
	int res, find;
	
	res = alloc_subnet_file(sf3, sf1->max_nr);
	if (res < 0)
		return -1;
	k = 0;
	for (i = 0; i < sf1->nr; i++) {
		find = 0;
		for (j = 0; j < sf2->nr; j++) {
			res = subnet_compare(&sf1->routes[i].subnet, &sf2->routes[j].subnet);
			if (res == INCLUDED || res == EQUALS) {
				st_debug(ADDRCOMP, 2, "skipping %P, included in %P\n", sf1->routes[i].subnet,
						sf2->routes[j].subnet);
				find = 1;
				break;
			}
		}
		if (find == 0) {
			copy_route(&sf3->routes[k], &sf1->routes[i]);
			k++;
		}
	}
	sf3->nr = k;
	return 0;
}
/**
 * files MUST be sorted
 * this is not a basic diff where LINE1 equals LINE2
 * if LINE1 != LINE2 we try to get a relation between the subnets
 * this func is borken
 */
void diff_files(const struct subnet_file *sf1, const struct subnet_file *sf2, struct st_options *nof) {
	unsigned long i = 0, j = 0;
	int a, res;
	unsigned long imax, jmax;
	u32 mask1, mask2;
	char buffer1[51];
	char buffer2[51];

	debug(ADDRCOMP, 3, "comparing %lu elements against %lu elements\n", sf1->nr, sf2->nr);
	while (i < sf1->nr||j < sf2->nr) {
		if ( i == sf1->nr && j == sf2->nr )
			break;

		if (i == sf1->nr) { /* FILE1 ended  print remaining FILE2 lines */
			mask2 = sf2->routes[j].subnet.mask;
			subnet2str(&sf2->routes[j].subnet, buffer2, sizeof(buffer2), 2);
			fprintf(nof->output_file, ";;;%s;%d\n", buffer2, mask2);
			j++;
			if (j == sf2->nr)
				 break;
			continue;
		}
		if (j == sf2->nr) { /* FILE2 ended  print remaining FILE1 lines */
			mask1 = sf1->routes[i].subnet.mask;
			subnet2str(&sf1->routes[i].subnet, buffer1, sizeof(buffer1), 2);
			printf("%s;%d;;;\n", buffer1, mask1);
			i++;
			if (i == sf1->nr)
				 break;
			continue;
		}
		mask1 = sf1->routes[i].subnet.mask;
		subnet2str(&sf1->routes[i].subnet, buffer1, sizeof(buffer1), 2);
		mask2 = sf2->routes[j].subnet.mask;
		subnet2str(&sf2->routes[j].subnet, buffer2, sizeof(buffer1), 2);
		a = 0;
		res = subnet_compare(&sf1->routes[i].subnet, &sf2->routes[j].subnet);
		debug(ADDRCOMP, 5, "i=%lu j=%lu comparing %s/%d  to %s/%d : %d \n", i, j, buffer1, mask1, buffer2, mask2, res);
		if (res == EQUALS)
			a = 1;
		if (res == INCLUDES||a == 1) {
			jmax = j;
			if (res == INCLUDES)
				fprintf(nof->output_file, "%s;%d;INCLUDES;%s;%d\n", buffer1, mask1, buffer2, mask2);
			while (1) { /** loop all included networks in FILE1(i) */
				jmax++;
				if (jmax == sf2->nr)
					break;
				subnet2str(&sf2->routes[jmax].subnet, buffer2, sizeof(buffer2), 2);
				mask2 = sf2->routes[jmax].subnet.mask;
				res = subnet_compare(&sf1->routes[i].subnet, &sf2->routes[jmax].subnet);
				debug(ADDRCOMP, 5, "i=%lu jmax=%lu comparing %s/%d  to %s/%d : %d \n", i, jmax, buffer1, mask1, buffer2, mask2, res);
				if (res != INCLUDES)
					break;
				fprintf(nof->output_file, "%s;%d;INCLUDES;%s;%d\n", buffer1, mask1, buffer2, mask2);
			}
			if (a == 0) {
				i++;
				continue;
			}
		}
		if (res == INCLUDED||a == 1) {
			imax = i;
			if (res == INCLUDED)
				fprintf(nof->output_file, "%s;%d;INCLUDED;%s;%d\n", buffer1, mask1, buffer2, mask2);
			while (1) { /** loop all included networks in FILE2(j) */
				imax++;
				if (imax == sf1->nr)
					break;

				subnet2str(&sf1->routes[imax].subnet, buffer1, sizeof(buffer1), 2);
				mask1 = sf1->routes[imax].subnet.mask;
				res   = subnet_compare(&sf1->routes[imax].subnet, &sf2->routes[j].subnet);
				debug(ADDRCOMP, 5, "imax=%lu j=%lu comparing %s/%d  to %s/%d : %d \n", i, jmax, buffer1, mask1, buffer2, mask2, res);
				if (res != INCLUDES)
					break;
				fprintf(nof->output_file, "%s;%d;INCLUDED;%s;%d\n", buffer1, mask1, buffer2, mask2);
			}
			if (a == 0) {
				j++;
				continue;
			}
		}
		if (a == 1) {
			j++, i++;
			continue;
		}
		debug(ADDRCOMP, 5, "Numeric comparing %s  to %s\n", buffer1, buffer2);
		res = subnet_is_superior(&sf1->routes[i].subnet, &sf2->routes[j].subnet);
		if (res == 0) { /* subnet(FILE1) > subnet(FILE2)*/
			j++;
			fprintf(nof->output_file, ";;;%s;%d\n", buffer2, mask2);
			while (1) { /* we increase until FILE2 is EQUALS or superior to FILE1 */
				mask2 = sf2->routes[j].subnet.mask;
				subnet2str(&sf2->routes[j].subnet, buffer2, sizeof(buffer2), 2);
				res = subnet_compare(&sf1->routes[i].subnet, &sf2->routes[j].subnet);
				if (res == EQUALS || res == INCLUDED )
					break;
				res = subnet_is_superior(&sf1->routes[i].subnet, &sf2->routes[j].subnet);
				if (res == 0)  {
					fprintf(nof->output_file, ";;;%s;%d\n", buffer2, mask2);
					j++;
				} else
					break;
			}
		} else {
			i++;
			fprintf(nof->output_file, "%s;%d;;;\n", buffer1, mask1);
			while (1) {
				mask1 = sf1->routes[i].subnet.mask;
				subnet2str(&sf1->routes[i].subnet, buffer1, sizeof(buffer1), 2);
				res = subnet_compare(&sf1->routes[i].subnet, &sf2->routes[j].subnet);
				if (res == EQUALS || res == INCLUDES)
					break;
				res = subnet_is_superior(&sf1->routes[i].subnet, &sf2->routes[j].subnet);
				if (res)  {
					fprintf(nof->output_file, "%s;%d;;;\n", buffer1, mask1);
					i++;
				} else
					break;
			}
		} // if res == 0
	}
}

/*
 *  loop through sf1 and match against PAIP/ IPAM
 */
void print_file_against_paip(struct subnet_file *sf1, const struct subnet_file *paip, struct st_options *nof) {
	u32 mask1, mask2;
	int res;
	unsigned long i, j;
	int find_included, find_equals;
	int includes;
	u32 find_mask;
	struct subnet find_subnet;
	char find_comment[128], buffer1[51], buffer2[51];

	debug_timing_start(2);
	for (i = 0; i < sf1->nr; i++) {
		mask1 = sf1->routes[i].subnet.mask;
		subnet2str(&sf1->routes[i].subnet, buffer1, sizeof(buffer1), 2);
		find_equals = 0;

		/** first try an exact match **/
		for (j = 0;  j < paip->nr; j++) {
			mask2 = paip->routes[j].subnet.mask;
			res = subnet_compare(&sf1->routes[i].subnet, &paip->routes[j].subnet);
			if (res == EQUALS) {
				strxcpy(sf1->routes[i].comment, paip->routes[j].comment, sizeof(sf1->routes[i].comment));
				fprint_route_fmt(nof->output_file, &sf1->routes[i], nof->output_fmt);
				find_equals = 1;
				break;
			}
		}
		if (find_equals == 1)
			continue;

		find_included = 0;
		includes = 0;
		find_mask = 0;
		strcpy(sf1->routes[i].comment, "NOT FOUND");
		fprint_route_fmt(nof->output_file, &sf1->routes[i], nof->output_fmt);

		/**
		 ** we look a second time for a match
 		 ** we could avoid doing 2 runs if both files are sorted
		 **
		 **/
		for (j = 0; j < paip->nr; j++) {
			mask2 = paip->routes[j].subnet.mask;
			subnet2str(&paip->routes[j].subnet, buffer2, sizeof(buffer2), 2);
			res = subnet_compare( &sf1->routes[i].subnet, &paip->routes[j].subnet);
			if (res == INCLUDED) {
				find_included = 1;
				/* we  get the largest including mask only */
				if (mask2 <  find_mask)
					continue;
				strxcpy(find_comment, paip->routes[j].comment, sizeof(find_comment));
				memcpy(&find_subnet, &paip->routes[j], sizeof(find_subnet));
				find_mask = mask2;
			} else if (includes > 5) {
				/* rate limite */
				continue;
			} else if (res == INCLUDES) {
				includes++;
				fprintf(nof->output_file, "###%s;%d includes %s;%d;%s\n", buffer1, mask1, buffer2, mask2, paip->routes[j].comment);
			}
		}
		if (find_included) {
			subnet2str(&find_subnet, buffer2, sizeof(buffer2), 2);
			fprintf(nof->output_file, "###%s;%d is included in  %s;%d;%s\n", buffer1, mask1, buffer2, find_mask, find_comment);
		}
	}
	debug_timing_end(2);
}

int network_grep_file(char *name, struct st_options *nof, char *ip) {
	char *s;
	char buffer[1024];
	char save_buffer[1024];
	FILE *f;
	struct subnet subnet, subnet1;
	int i, res, find_ip, do_compare, reevaluate;
	unsigned long line = 0;

	if (name == NULL)
		return -1;
	f = fopen(name, "r");
	if (f == NULL) {
		fprintf(stderr, "error: cannot open %s for reading\n", name);
		return -2;
	}
	res = get_subnet_or_ip(ip, &subnet1);
	if (res < 0) {
		fprintf(stderr, "WTF? \"%s\"  IS  a prefix/mask ?? really?\n", ip);
		return -3;
	}
	debug_timing_start(2);
	while ((s = fgets(buffer, sizeof(buffer), f))) {
		line++;
		debug(GREP, 9, "grepping line %lu : %s\n", line, s);
		strcpy(save_buffer, buffer);
		s = strtok(s, nof->delim);
		if (s == NULL)
			continue;
		do_compare = 0;
		find_ip    = 0;
		reevaluate = 1;
		if (nof->grep_field > 0) { /** we grep on only one field */
			res = 0;
			for (i = 0; i < nof->grep_field - 1; i++) {
				s = strtok(NULL, nof->delim);
				if (s == NULL) {
					debug(GREP, 3, "no token at offset %d line %lu\n", nof->grep_field, line);
					res = 1;
					break;
				}
			}
			if (res)
				continue; /* invalid line, continue **/
		}

		do {
			if (reevaluate == 0)
				s = strtok(NULL, nof->delim);
			reevaluate = 0;
			if (s == NULL)
				break;
			if (find_ip) { /* previous token was an IP without a mask, try to get a mask on this field*/
				int lmask = string2mask(s, 21);
				if (subnet.ip_ver == IPV4_A)
					subnet.mask = (lmask == BAD_MASK ? 32 : lmask);
				if (subnet.ip_ver == IPV6_A)
					subnet.mask = (lmask == BAD_MASK ? 128 : lmask);
				/* if no mask found, we assume the found IP was a host */
				find_ip     = 0;
				do_compare  = 1;
				if (lmask == BAD_MASK)
					reevaluate = 1; /* we need to reeavulate s, it could be an IP */
			} else {
				res = get_subnet_or_ip(s, &subnet);
				if (res < 0 && nof->grep_field)  {
					debug(GREP, 2, "field %s line %lu not an IP, bad grep_offset value???\n",  s, line);
					break;
				} else if (res > 1000) {/* c pas une IP */
					debug(GREP, 5, "field %s line %lu not an IP\n",  s, line);
					continue;
				} else if (res == IPV4_A) { /* une IP sans masque */
					find_ip = 1;
					debug(GREP, 5, "field %s line %lu IS an IP\n",  s, line);
				} else if (res == IPV4_N) { /* IP+masque, time to compare */
					do_compare = 1;
					debug(GREP, 5, "field %s line %lu IS a IP/mask \n",  s, line);
				} else if (res == IPV6_N) {
					do_compare = 1;
					debug(GREP, 5, "field %s line %lu IS a IPv6/mask \n",  s, line);
				 } else if (res == IPV6_A) {
					find_ip = 1;
					debug(GREP, 5, "field %s line %lu IS an IPv6\n",  s, line);
				}
			}

			if (do_compare == 1) {
				 switch (subnet_compare(&subnet,  &subnet1)) {
					case EQUALS:
						fprintf(nof->output_file, "%s", save_buffer);
						debug(GREP, 1, "field %s line %lu equals\n",  s, line);
						do_compare = 2;
						break;
					case INCLUDES:
						fprintf(nof->output_file, "%s", save_buffer);
						debug(GREP, 1, "field %s line %lu includes\n",  s, line);
						do_compare = 2;
						break;
					case INCLUDED:
						fprintf(nof->output_file, "%s", save_buffer);
						debug(GREP, 1, "field %s line %lu included\n",  s, line);
						do_compare = 2;
						break;
					default:
						do_compare = 0;
						break;
				 }
			}
			if (do_compare == 2) /*  one match found, next line please */
				break;
			if (nof->grep_field && find_ip == 0)
				break; /* no match found but we wanted only THIS field */
		} while (1);
	} // for fgets
	fclose(f);
	debug_timing_end(2);
	return 0;
}

static int __heap_subnet_is_superior(void *v1, void *v2) {
	struct subnet *s1 = &((struct route *)v1)->subnet;
	struct subnet *s2 = &((struct route *)v2)->subnet;
	return subnet_is_superior(s1, s2);
}

static void __heap_print_subnet(void *v) {
	struct subnet *s = &((struct route*)v)->subnet;

	st_printf("%P", *s);
}
/*
 * simplify subnet file (removes redundant entries)
 * GW is not taken into account
 */
int subnet_file_simplify(struct subnet_file *sf) {
	unsigned long i;
	int  res;
	TAS tas;
	struct route *new_r, *r;

	if (sf->nr == 0)
		return 0;
	debug_timing_start(2);
	alloc_tas(&tas, sf->nr, __heap_subnet_is_superior);

	new_r = malloc(sf->nr * sizeof(struct route));
	if (tas.tab == NULL || new_r == NULL) {
		fprintf(stderr, "%s : no memory \n", __FUNCTION__);
		return -1;
	}
	debug(MEMORY, 2, "Allocated %lu bytes for new struct route\n", sf->nr * sizeof(struct route));
	for (i = 0; i < sf->nr; i++)
		addTAS(&tas, &sf->routes[i]);

	r = popTAS(&tas);
	copy_route(&new_r[0], r);
	i = 1;
	while (1) {
                r = popTAS(&tas);
                if (r == NULL)
                        break;
		/* because the 'new_r' list is sorted, we know the only network to consider is i - 1 */
		res = subnet_compare(&r->subnet, &new_r[i - 1].subnet);
		if (res == INCLUDED || res == EQUALS ) {
			st_debug(ADDRCOMP, 3, "%P is included in %P, skipping\n", r->subnet, new_r[i - 1].subnet);
			continue;
		}
		copy_route(&new_r[i], r);
		i++;
        }
	sf->max_nr = sf->nr;
        sf->nr = i;
	free(tas.tab);
	free(sf->routes);
	sf->routes = new_r;
	debug_timing_end(2);
	return 0;
}

/*
 * simply_route_file takes GW into account, must be equal
 */
int route_file_simplify(struct subnet_file *sf,  int mode) {
	unsigned long i, j ,a;
	int res, skip;
	TAS tas;
	struct route *new_r, *r, *discard;

	alloc_tas(&tas, sf->nr, __heap_subnet_is_superior);
	new_r   =  malloc(sf->nr * sizeof(struct route)); /* common routes */
	discard =  malloc(sf->nr * sizeof(struct route)); /* excluded routes */

	debug(MEMORY, 2, "Allocated %lu bytes for new struct route\n", 2 * sf->nr * sizeof(struct route));
	if (tas.tab == NULL||new_r == NULL||discard == NULL) {
		fprintf(stderr, "%s : no memory\n", __FUNCTION__);
		return -1;
	}
	for (i = 0; i < sf->nr; i++)
		addTAS(&tas, &sf->routes[i]);

	r = popTAS(&tas);
	copy_route(&new_r[0], r);
	i = 1; /* index in the 'new_r' struct */
	j = 0; /* index in the 'discard' struct */
	while (1) {
                r = popTAS(&tas);
                if (r == NULL)
                        break;
		a = i - 1;
		skip = 0;
		while (1) {
			res = subnet_compare(&r->subnet, &new_r[a].subnet);
			if (res == INCLUDED || res == EQUALS) {
					/* because the 'new_r' list is sorted, we know the first 'backward' match is the longest one
					 * and the longest match is the one that matters
					 */
					if (is_equal_gw(r, &new_r[a])) {
						st_debug(ADDRCOMP, 3, "%P is included in %P, discarding it\n", r->subnet, new_r[a].subnet);
						skip = 1;
					} else {
						st_debug(ADDRCOMP, 3, "%P is included in %P but GW is different, keeping it\n", r->subnet, new_r[a].subnet);
					}
					break;
			}
			if (a == 0)
				break;
			a--;
		}
		if (skip == 0)
			copy_route(&new_r[i++], r);
		else
			copy_route(&discard[j++], r);
        }
	free(tas.tab);
	free(sf->routes);
	sf->max_nr = sf->nr;
	if (mode == 0) {
        	sf->nr = i;
		sf->routes = new_r;
		free(discard);
	} else {
        	sf->nr = j;
		sf->routes = discard;
		free(new_r);
	}
	return 1;
}

/*
 * mode == 1 means we take the GW into acoount
 * mode == 0 means we dont take the GW into account
 */
int aggregate_route_file(struct subnet_file *sf, int mode) {
	unsigned long i, j;
	int res;
	struct subnet s;
	struct route *new_r;

	/* first, remove duplicates and sort the crap*/
	debug_timing_start(2);
	res = subnet_file_simplify(sf);
	if (res < 0) {
		debug_timing_end(2);
		return res;
	}
	new_r = malloc(sf->nr * sizeof(struct route));
	if (new_r == NULL) {
		fprintf(stderr, "%s : no memory\n", __FUNCTION__);
		debug_timing_end(2);
		return -1;
	}
	debug(MEMORY, 2, "Allocated %lu bytes for new struct route\n", sf->nr * sizeof(struct route));
	copy_route(&new_r[0], &sf->routes[0]);
	j = 0; /* i is the index in the original file, j is the index in the file we are building */
	for (i = 1; i < sf->nr; i++) {
		if (mode == 1 && !is_equal_gw(&new_r[j],  &sf->routes[i])) {
			st_debug(AGGREGATE, 4, "Entry %lu [%P] & %lu [%P] cant aggregate, different GW\n", j, new_r[j].subnet,
					i, sf->routes[i].subnet);
			j++;
			copy_route(&new_r[j], &sf->routes[i]);
			continue;
		}
		res = aggregate_subnet(&new_r[j].subnet, &sf->routes[i].subnet, &s);
		if (res < 0) {
			st_debug(AGGREGATE, 4, "Entry %lu [%P] & %lu [%P] cant aggregate\n", j, new_r[j].subnet,
					i, sf->routes[i].subnet);
			j++;
			copy_route(&new_r[j], &sf->routes[i]);
			continue;
		}
		st_debug(AGGREGATE, 4, "Entry %lu [%P] & %lu [%P] can aggregate\n", j, new_r[j].subnet,
			i, sf->routes[i].subnet);
		copy_subnet(&new_r[j].subnet, &s);
		if (mode == 1)
			copy_ipaddr(&new_r[j].gw, &sf->routes[i].gw);
		else
			memset(&new_r[j].gw, 0, sizeof(struct ip_addr)); /* the aggregate route has null gateway */
		strcpy(new_r[j].comment, "AGGREGATE");
		/* rewinding and aggregating backwards as much as we can; the aggregate we just created may aggregate with j - 1 */
		while (j > 0) {
			if (mode == 1 && !is_equal_gw(&new_r[j], &new_r[j - 1]))
				break;
			res = aggregate_subnet(&new_r[j].subnet, &new_r[j - 1].subnet, &s);
			if (res >= 0) {
				st_debug(AGGREGATE, 4, "Rewinding, entry %lu [%P] & %lu [%P] can aggregate\n", j - 1, new_r[j - 1].subnet, j, new_r[j].subnet);
				j--;
				copy_subnet(&new_r[j].subnet, &s);
				if (mode == 1)
					copy_ipaddr(&new_r[j].gw, &sf->routes[i].gw);
				else
					memset(&new_r[j].gw, 0, sizeof(struct ip_addr)); /* the aggregate route has null gateway */
				strcpy(new_r[j].comment, "AGGREGATE");
			} else
				break;
		} /* while j */
	} /* for i */
	free(sf->routes);
	sf->routes = new_r;
	sf->max_nr = sf->nr;
	sf->nr = j + 1;
	debug_timing_end(2);
	return 1;
}

int subnet_file_merge_common_routes(const struct subnet_file *sf1,  const struct subnet_file *sf2,
		struct subnet_file *sf3) {
	unsigned long  i, j, can_add;
	int res;
	struct route *r;
	TAS tas;

	debug_timing_start(2);
	alloc_tas(&tas, sf1->nr + sf2->nr, &__heap_subnet_is_superior);
	if (tas.tab == NULL||sf3->routes == NULL) {
		fprintf(stderr, "%s : no memory\n", __FUNCTION__);
		debug_timing_end(2);
		return -1;
	}
	/* going through subnet_file1 ; adding to stack only if equals or included */
	for (i = 0; i < sf1->nr; i++) {
		for (j = 0; j < sf2->nr; j++) {
			res = subnet_compare(&sf1->routes[i].subnet, &sf2->routes[j].subnet);
			if (res == INCLUDED || res == EQUALS) {
				st_debug(ADDRCOMP, 3, "Loop #1 adding %P (included in : %P)\n", sf1->routes[i].subnet,
						sf2->routes[j].subnet);
				addTAS(&tas, &sf1->routes[i]);
				break;
			}
		}
	}
	/* going through subnet_file2 ; adding to stack only if INCLUDED */
	for (j = 0; j < sf2->nr; j++) {
		can_add = 0;
		for (i = 0; i < sf1->nr; i++) {
			res = subnet_compare(&sf2->routes[j].subnet, &sf1->routes[i].subnet);
			if (res == INCLUDED) {
				st_debug(ADDRCOMP, 3, "Loop #2 may add %P (included in : %P)\n", sf2->routes[j].subnet,
						sf1->routes[i].subnet);
				can_add = 1;
			} else if (res == EQUALS) {/* already added, skipping */
				can_add = 0; /* maybe the route we are matching is included in something; in that case we wont add it again */
				break;
			}
		}
		if (can_add) {
			st_debug(ADDRCOMP, 3, "Loop #2 add %P\n", sf2->routes[j].subnet);
			addTAS(&tas, &sf2->routes[j]);
		}
	}
	for (i = 0; ; i++) {
		r = popTAS(&tas);
		if (r == NULL)
			break;
		copy_route(&sf3->routes[i], r);
	}
	sf3->nr = i;
	free(tas.tab);
	debug_timing_end(2);
	return 1;
}

unsigned long long sum_subnet_file(struct subnet_file *sf) {
	unsigned long i;
	int res;
	unsigned long long sum = 0;
	int ipver;

	ipver = sf->routes[0].subnet.ip_ver;
	res = subnet_file_simplify(sf);
	if (res < 0)
		return 0;
	for (i = 0; i < sf->nr; i++) {
		if (sf->routes[i].subnet.ip_ver != ipver) /* cant add IPv4 to IPv6, can we*/
			continue;
		if (ipver == IPV4_A)
			sum += 1ULL << (32 - sf->routes[i].subnet.mask); /*power of 2  */
		if (ipver == IPV6_A) { /* we count only /64 not single host; hosts are unlimited in IPv6 (2^64)  */
			if (sf->routes[i].subnet.mask <= 64)
				sum += 1ULL << (64 - sf->routes[i].subnet.mask);
		}
	}
	return sum;
}

int subnet_sort_ascending2(struct subnet_file *sf) {
	unsigned long i;
	TAS tas;
	struct route *new_r, *r;

	if (sf->nr == 0)
		return 0;
	debug_timing_start(2);
	alloc_tas(&tas, sf->nr, __heap_subnet_is_superior);
	tas.print = &__heap_print_subnet;

	new_r = malloc(sf->max_nr * sizeof(struct route));

	if (tas.tab == NULL||new_r == NULL) {
		fprintf(stderr, "%s : no memory\n", __FUNCTION__);
		debug_timing_end(2);
		return -1;
	}
	debug(MEMORY, 2, "Allocated %lu bytes for new struct route\n", sf->max_nr * sizeof(struct route));
	/* basic heapsort */
	for (i = 0 ; i < sf->nr; i++)
		addTAS(&tas, &(sf->routes[i]));
	for (i = 0 ; i < sf->nr; i++) {
		r = popTAS(&tas);
		copy_route(&new_r[i], r);
	}
	free(tas.tab);
	free(sf->routes);
	sf->routes = new_r;
	debug_timing_end(2);
	return 0;
}
/*
 * result is stored in *sf2
 */
int subnet_file_remove(const struct subnet_file *sf1, struct subnet_file *sf2, const struct subnet *subnet) {
	unsigned long i, j;
	int res, n;
	struct subnet *r;
	struct route *new_r;
	
	j = 0;
	res = alloc_subnet_file(sf2, sf1->max_nr);
	if (res < 0)
		return res;
	for (i = 0; i < sf1->nr; i++) {
		res = subnet_compare(&sf1->routes[i].subnet, subnet);
		if (res == NOMATCH || res == INCLUDED) {
			copy_route(&sf2->routes[j],  &sf1->routes[i]);
			j++;
			st_debug(ADDRREMOVE, 2, "%P is not included in %P\n", *subnet, sf1->routes[i]);
			continue;
		} else if (res == EQUALS) {
			st_debug(ADDRREMOVE, 2, "removing entire subnet %P\n", *subnet);
			continue;
		}
		r = subnet_remove(&sf1->routes[i].subnet, subnet, &n);
		if (n == -1) {
			fprintf(stderr, "%s : no memory\n", __FUNCTION__);
			return n;
		}
		/* realloc memory if necessary */
		if (n + sf2->nr >= sf2->max_nr) {
			sf2->max_nr *= 2;
			new_r = realloc(sf2->routes,  sizeof(struct route) * sf2->max_nr);
			if (new_r == NULL) {
				fprintf(stderr, "unable to reallocate, need to abort\n");
				return -3;
			}
			sf2->routes = new_r;
		} /* realloc */
		for (res = 0; res < n; res++) {
			copy_route(&sf2->routes[j],  &sf1->routes[i]);
			copy_subnet(&sf2->routes[j].subnet, &r[res]);
			j++;
		}
		free(r);
	}
	sf2->nr = j;
	return 1;
}

/*
 * parse splits levels
 * levels are M,N,O like 2,4,8 meaning split in 2, then split the result in 4, then split the result in 8
 */
static int split_parse_levels(char *s, int *levels) {
	int i = 0, current = 0;
	int n_levels = 0;

	if (!isdigit(s[i])) {
		debug(SPLIT, 1, "Invalid level string '%s', starts with '%c'\n", s, s[0]);
		return -1;
	}
	while (1) {
		if (s[i] == '\0')
			break;
		if (s[i] == ',' && s[i + 1] == ',') {
			debug(SPLIT, 1, "Invalid level string '%s', 2 consecutives ','\n", s);
			return -1;
		} else if (s[i] == ',' && s[i + 1] == '\0') {
                        debug(SPLIT, 1, "Invalid level string '%s', ends with','\n", s);
                        return -1;
		}
		if (s[i] == ',') {
			if (!isPower2(current)) {
				debug(SPLIT, 1, "split level %d is not a power of two\n", current);
				return -1;
			}
			levels[n_levels] = current;
			debug(SPLIT, 5, "split level#%d = %d\n", n_levels, levels[n_levels]);
			n_levels++;
			if (n_levels == 8) {
				debug(SPLIT, 1, "only 10 split levels are supported\n");
				return n_levels;
			}
			current = 0;
		} else if (isdigit(s[i])) {
			current *= 10;
			current += (s[i] - '0');
		} else {
			debug(SPLIT, 1, "Invalid level string '%s', 2 consecutives ','\n", s);
			return -1;
		}
		i++;
	}
	if (!isPower2(current)) {
		debug(SPLIT, 1, "split level %d is not a power of two\n", current);
		return -1;
	}
	levels[n_levels] = current;
	debug(SPLIT, 5, "split level#%d = %d\n", n_levels, levels[n_levels]);
	n_levels++;
	return n_levels;
}

static int sum_log_to(int *level, int n, int max_n) {
	int i, res = 0;

	for (i = n; i < max_n; i++)
		res += mylog2(level[i]);
	return res;
}

/* split n,m means split 's' n times, and each resulting subnet m times */
int subnet_split(FILE *out, const struct subnet *s, char *string_levels) {
	int k, res;
	int levels[12];
	int n_levels;
	struct subnet subnet;
	int base_mask;
	unsigned long sum = 0, i = 0;

	res = split_parse_levels(string_levels, levels);
	if (res < 0)
		return res;
	n_levels = res;
	res = sum_log_to(levels, 0, n_levels);
	/* make sure the splits levels are not too large; for IPv6, we can loop more than 2^(ulong bits) */
	if  ((s->ip_ver == IPV4_A && res > (32 - s->mask))
			|| (s->ip_ver == IPV6_A && res > (128 - s->mask)) || res > sizeof(sum) * 4) {
		debug(SPLIT, 1, "Too many splits\n");
		return -1;
	}
	sum = 1;
	/* calculate the number of time we need to loop */
	for (i = 0; i < n_levels; i++)
		sum *= levels[i];
	copy_subnet(&subnet, s);
	base_mask = subnet.mask;

	i = 0;
	while (i < sum) {
		for (k = 0; k < n_levels - 1; k++) {
			subnet.mask = base_mask + sum_log_to(levels, 0, k + 1);
			st_fprintf(out, "%N/%m;", subnet, subnet);
		}
		subnet.mask = base_mask + sum_log_to(levels, 0, k + 1);
		st_fprintf(out, "%N/%m\n", subnet, subnet);
		next_subnet(&subnet);
		i++;
	}
	return 1;
}

/*
 * parse splits levels, second version
 * levels are M,N,O like 24,28,30 meaning split in /24, then split the result in /28, then split the result in 
 * /30
 */
static int split_parse_levels_2(char *s, int *levels) {
	int i = 0, current = 0;
	int n_levels = 0;

	if (!isdigit(s[i])) {
		debug(SPLIT, 1, "Invalid level string '%s', starts with '%c'\n", s, s[0]);
		return -1;
	}
	while (1) {
		if (s[i] == '\0')
			break;
		if (s[i] == ',' && s[i + 1] == ',') {
			debug(SPLIT, 1, "Invalid level string '%s', 2 consecutives ','\n", s);
			return -1;
		} else if (s[i] == ',' && s[i + 1] == '\0') {
                        debug(SPLIT, 1, "Invalid level string '%s', ends with','\n", s);
                        return -1;
		}
		if (s[i] == ',') {
			levels[n_levels] = current;
			if (n_levels && current <= levels[n_levels - 1]) {
				debug(SPLIT, 1, "Invalid split, %d >= %d\n", levels[n_levels - 1], current);
				return -1;
			}
			debug(SPLIT, 5, "split level#%d = %d\n", n_levels, levels[n_levels]);
			n_levels++;
			if (n_levels == 8) {
				debug(SPLIT, 1, "only 10 split levels are supported\n");
				return n_levels;
			}
			current = 0;
		} else if (isdigit(s[i])) {
			current *= 10;
			current += (s[i] - '0');
		} else {
			debug(SPLIT, 1, "Invalid level string '%s', 2 consecutives ','\n", s);
			return -1;
		}
		i++;
	}
	levels[n_levels] = current;
	if (n_levels && current <= levels[n_levels - 1]) {
		debug(SPLIT, 1, "Invalid split, %d >= %d\n", levels[n_levels - 1], current);
		return -1;
	}
	debug(SPLIT, 5, "split level#%d = %d\n", n_levels, levels[n_levels]);
	n_levels++;
	return n_levels;
}

/* split n,m means split 's' n times, and each resulting subnet m times */
int subnet_split_2(FILE *out, const struct subnet *s, char *string_levels) {
	unsigned long int i = 0;
	int k, res;
	int levels[12];
	int n_levels;
	struct subnet subnet;
	unsigned long  sum = 0;

	res = split_parse_levels_2(string_levels, levels);
	if (res < 0)
		return res;
	if (levels[0] <= s->mask) {
		debug(SPLIT, 1, "split mask lower than subnet mask\n");
		return -1;
	}
	n_levels = res;
	if  ((s->ip_ver == IPV4_A && levels[res - 1] > 32)
			|| (s->ip_ver == IPV6_A && levels[res - 1] > 128)
			|| (levels[n_levels - 1] - s->mask >=  sizeof(sum) * 8)) {
		debug(SPLIT, 1, "Too many splits\n");
		return -1;
	}
	sum = 1 << (levels[0] - s->mask);
	/* calculate the number of time we need to loop */
	for (i = 1; i < n_levels; i++)
		sum *= (1 << (levels[i] - levels[i - 1]));
	copy_subnet(&subnet, s);

	i = 0;
	while (i < sum) {
		for (k = 0; k < n_levels - 1; k++) {
			subnet.mask = levels[k];
			st_fprintf(out, "%N/%m;", subnet, subnet);
		}
		subnet.mask = levels[k];
		st_fprintf(out, "%N/%m\n", subnet, subnet);
		next_subnet(&subnet);
		i++;
	}
	return 1;
}

static int __heap_gw_is_superior(void *v1, void *v2) {
	struct subnet *s1 = &((struct route *)v1)->subnet;
	struct subnet *s2 = &((struct route *)v2)->subnet;
	struct ip_addr *gw1 = &((struct route *)v1)->gw;
	struct ip_addr *gw2 = &((struct route *)v2)->gw;

	if (is_equal_ip(gw1, gw2))
		return subnet_is_superior(s1, s2);
	else
		return addr_is_superior(gw1, gw2);
}

static int __heap_mask_is_superior(void *v1, void *v2) {
	struct subnet *s1 = &((struct route *)v1)->subnet;
	struct subnet *s2 = &((struct route *)v2)->subnet;

	if (s1->mask == s2->mask)
		return subnet_is_superior(s1, s2);
	else
		return (s1->mask < s2->mask);
}

static int __subnet_sort_by(struct subnet_file *sf, int cmpfunc(void *v1, void *v2)) {
	unsigned long i;
	TAS tas;
	struct route *new_r, *r;

	if (sf->nr == 0)
		return 0;
	debug_timing_start(2);
	alloc_tas(&tas, sf->nr, cmpfunc);

	new_r = malloc(sf->max_nr * sizeof(struct route));

	if (tas.tab == NULL || new_r == NULL) {
		fprintf(stderr, "%s : no memory\n", __FUNCTION__);
		debug_timing_end(2);
		return -1;
	}
	debug(MEMORY, 2, "Allocated %lu bytes for new struct route\n", sf->max_nr * sizeof(struct route));
	/* basic heapsort */
	for (i = 0 ; i < sf->nr; i++)
		addTAS(&tas, &(sf->routes[i]));
	for (i = 0 ; i < sf->nr; i++) {
		r = popTAS(&tas);
		copy_route(&new_r[i], r);
	}
	free(tas.tab);
	free(sf->routes);
	sf->routes = new_r;
	debug_timing_end(2);
	return 0;
}

struct subnetsort {
	char *name;
	int (*cmpfunc)(void *v1, void *v2);
};

static const struct subnetsort subnetsort[] = {
	{ "prefix",	&__heap_subnet_is_superior },
	{ "gw",		&__heap_gw_is_superior },
	{ "mask",	&__heap_mask_is_superior },
	{NULL,		NULL}
};

void subnet_available_cmpfunc(FILE *out) {
	int i = 0;

	while (1) {
		if (subnetsort[i].name == NULL)
			break;
		fprintf(out, " %s\n", subnetsort[i].name);
		i++;
	}
}

int subnet_sort_by(struct subnet_file *sf, char *name) {
	int i = 0;

	while (1) {
		if (subnetsort[i].name == NULL)
			break;
		if (!strncasecmp(name, subnetsort[i].name, strlen(name)))
			return __subnet_sort_by(sf, subnetsort[i].cmpfunc);
		i++;
	}
	return -1664;
}

int subnet_sort_ascending(struct subnet_file *sf) {
	return __subnet_sort_by(sf, __heap_subnet_is_superior);
}
