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
#include "st_printf.h"
#include "generic_expr.h"
#include "st_scanf.h"
#include "st_handle_csv_files.h"
#include "subnet_tool.h"

/*
 * compare 2 CSV files sf1 and sf1
 * prints sf1 subnets, and subnet from sf2 that are equals or included
 */
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
 * get uniques routes from sf1 and sf2 INTO sf3
 **/
int uniq_routes(const struct subnet_file *sf1, const struct subnet_file *sf2, struct subnet_file *sf3) {
	unsigned long i, j;
	int res, find;
	TAS tas;
	struct route *r;

	res = alloc_subnet_file(sf3, sf2->nr + sf1->nr);
	if (res < 0)
		return res;
	res = alloc_tas(&tas, sf3->max_nr, __heap_subnet_is_superior);
	if (res < 0) { /* out of mem */
		free(sf3->routes);
		return res;
	}

	for (i = 0; i < sf1->nr; i++) {
		find = 0;
		for (j = 0; j < sf2->nr; j++) {
			res = subnet_compare(&sf1->routes[i].subnet, &sf2->routes[j].subnet);
			if (res != NOMATCH) {
				st_debug(ADDRCOMP, 4, "skipping %P, relaled with %P\n", sf1->routes[i].subnet,
						sf2->routes[j].subnet);
				find = 1;
				break;
			}
		}
		if (find == 0)
			addTAS(&tas, &sf1->routes[i]);
	}
	for (j = 0; j < sf2->nr; j++) {
		find = 0;
		for (i = 0; i < sf1->nr; i++) {
			res = subnet_compare(&sf2->routes[j].subnet, &sf1->routes[i].subnet);
			if (res != NOMATCH) {
				st_debug(ADDRCOMP, 4, "skipping %P rel with %P\n", sf2->routes[j].subnet,
						sf1->routes[i].subnet);
				find = 1;
				break;
			}
		}
		if (find == 0)
			addTAS(&tas, &sf2->routes[j]);
	}
	for (i = 0; ; i++) {
		r = popTAS(&tas);
		if (r == NULL)
			break;
		copy_route(&sf3->routes[i], r);
	}
	sf3->nr = i;
	free(tas.tab);
	return 1;
}

/*
 * get routes from sf1 not covered by sf2 INTO sf3
 **/
int missing_routes(const struct subnet_file *sf1, const struct subnet_file *sf2, struct subnet_file *sf3) {
	unsigned long i, j, k;
	int res, find;
	
	res = alloc_subnet_file(sf3, sf1->max_nr);
	if (res < 0)
		return res;
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
	return 1;
}

/*
 *  loop through sf1 and match against PAIP/ IPAM
 */
void print_file_against_paip(struct subnet_file *sf1, const struct subnet_file *paip, struct st_options *nof) {
	int mask;
	int res;
	unsigned long i, j;
	int find_included, find_equals, find_j;
	int includes;
	u32 find_mask;

	debug_timing_start(2);
	for (i = 0; i < sf1->nr; i++) {
		find_equals = 0;
		/** first try an exact match **/
		for (j = 0;  j < paip->nr; j++) {
			mask = paip->routes[j].subnet.mask;
			res = subnet_compare(&sf1->routes[i].subnet, &paip->routes[j].subnet);
			if (res == EQUALS) {
				strxcpy(sf1->routes[i].comment, paip->routes[j].comment,
						sizeof(sf1->routes[i].comment));
				fprint_route_fmt(nof->output_file, &sf1->routes[i], nof->output_fmt);
				find_equals = 1;
				break;
			}
		}
		if (find_equals)
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
			mask = paip->routes[j].subnet.mask;
			res = subnet_compare( &sf1->routes[i].subnet, &paip->routes[j].subnet);
			if (res == INCLUDED) {
				find_included = 1;
				/* we  get the largest including mask only */
				if (mask <  find_mask)
					continue;
				find_j = j;
				find_mask = mask;
			} else if (includes > 5) {
				/* rate limite */
				continue;
			} else if (res == INCLUDES) {
				includes++;
				st_fprintf(nof->output_file, "###%I;%d includes %I;%d;%s\n",
						sf1->routes[i].subnet, sf1->routes[i].subnet.mask,
						paip->routes[j].subnet, mask, paip->routes[j].comment);
			}
		}
		if (find_included) {
			st_fprintf(nof->output_file, "###%I;%d is included in  %I;%d;%s\n",
					sf1->routes[i].subnet, sf1->routes[i].subnet.mask,
					paip->routes[find_j].subnet, find_mask, paip->routes[find_j].comment);
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
	return 1;
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
	res = alloc_tas(&tas, sf->nr, __heap_subnet_is_superior);
	if (res < 0)
		return -1;
	new_r = malloc(sf->nr * sizeof(struct route));
	if (new_r == NULL) {
		free(tas.tab);
		fprintf(stderr, "%s : no memory \n", __FUNCTION__);
		return -1;
	}
	debug(MEMORY, 3, "Allocated %lu bytes for new struct route\n", sf->nr * sizeof(struct route));
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
	return 1;
}

/*
 * simply_route_file takes GW into account, must be equal
 */
int route_file_simplify(struct subnet_file *sf,  int mode) {
	unsigned long i, j ,a;
	int res, skip;
	TAS tas;
	struct route *new_r, *r, *discard;

	res = alloc_tas(&tas, sf->nr, __heap_subnet_is_superior);
	if (res < 0)
		return res;
	new_r   =  malloc(sf->nr * sizeof(struct route)); /* common routes */
	if (new_r == NULL) {
		free(tas.tab);
		fprintf(stderr, "%s : no memory\n", __FUNCTION__);
		return -1;
	}
	discard =  malloc(sf->nr * sizeof(struct route)); /* excluded routes */
	if (discard == NULL) {
		free(tas.tab);
		free(new_r);
		fprintf(stderr, "%s : no memory\n", __FUNCTION__);
		return -1;
	}
	debug(MEMORY, 3, "Allocated %lu bytes for new struct route\n", 2 * sf->nr * sizeof(struct route));

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
	debug(MEMORY, 3, "Allocated %lu bytes for new struct route\n", sf->nr * sizeof(struct route));
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
			zero_ipaddr(&new_r[j].gw); /* the aggregate route has null gateway */
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
					zero_ipaddr(&new_r[j].gw); /* the aggregate route has null gateway */
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
	res = alloc_tas(&tas, sf1->nr + sf2->nr, &__heap_subnet_is_superior);
	if (res < 0) {
		debug_timing_end(2);
		return res;
	}
	res = alloc_subnet_file(sf3, sf1->nr + sf2->nr);
	if (res < 0) {
		free(tas.tab);
		debug_timing_end(2);
		return res;
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

/*
 * result is stored in *sf2
 */
int subnet_file_remove_subnet(const struct subnet_file *sf1, struct subnet_file *sf2, const struct subnet *subnet) {
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
			st_debug(ADDRREMOVE, 4, "%P is not included in %P\n", *subnet, sf1->routes[i]);
			continue;
		} else if (res == EQUALS) {
			st_debug(ADDRREMOVE, 4, "removing entire subnet %P\n", *subnet);
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
			debug(MEMORY, 3, "reallocating %lu bytes for new_r\n", sizeof(struct route) * sf2->max_nr);
			if (new_r == NULL) {
				fprintf(stderr, "%s : unable to reallocate, need to abort\n", __FUNCTION__);
				return -3;
			}
			sf2->routes = new_r;
		} /* realloc */
		for (res = 0; res < n; res++) {
			copy_route(&sf2->routes[j],  &sf1->routes[i]); /* copy comment, device ... */
			copy_subnet(&sf2->routes[j].subnet, &r[res]);
			j++;
		}
		free(r);
	}
	sf2->nr = j;
	return 1;
}

/*
 * subnets from sf3 are removed from sf1
 * result is stored in *sf2 BUGGED
 */
int subnet_file_remove_file(struct subnet_file *sf1, struct subnet_file *sf2, const struct subnet_file *sf3) {
	unsigned long i;
	int res;
	struct subnet_file sf;

	debug_timing_start(2);
	memcpy(&sf, sf1, sizeof(struct subnet_file));

	for (i = 0; i < sf3->nr; i++) {
		st_debug(ADDRREMOVE, 4, "Loop %d, Removing %P\n", i, sf3->routes[i].subnet);
		res = subnet_file_remove_subnet(&sf, sf2, &sf3->routes[i].subnet);
		if (res < 0) {
			debug_timing_end(2);
			return res;
		}
		free(sf.routes);
		memcpy(&sf, sf2, sizeof(struct subnet_file));
	}
	subnet_file_simplify(sf2);
	debug_timing_end(2);
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
		fprintf(stderr, "Invalid level string '%s', starts with '%c'\n", s, s[0]);
		return -1;
	}
	while (1) {
		if (s[i] == '\0')
			break;
		if (s[i] == ',' && s[i + 1] == ',') {
			fprintf(stderr, "Invalid level string '%s', 2 consecutives ','\n", s);
			return -1;
		} else if (s[i] == ',' && s[i + 1] == '\0') {
                        fprintf(stderr, "Invalid level string '%s', ends with','\n", s);
                        return -1;
		}
		if (s[i] == ',') {
			if (!isPower2(current)) {
				fprintf(stderr, "split level %d is not a power of two\n", current);
				return -1;
			}
			levels[n_levels] = current;
			debug(SPLIT, 5, "split level#%d = %d\n", n_levels, levels[n_levels]);
			n_levels++;
			if (n_levels == 8) {
				fprintf(stderr, "Only 10 split levels are supported\n");
				return n_levels;
			}
			current = 0;
		} else if (isdigit(s[i])) {
			current *= 10;
			current += (s[i] - '0');
		} else {
			fprintf(stderr, "Invalid level string '%s', invalid char '%c'\n", s, s[i]);
			return -1;
		}
		i++;
	}
	if (!isPower2(current)) {
		fprintf(stderr, "split level %d is not a power of two\n", current);
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

/* split subnet 's' 'string_levels' times
 * split n,m means split 's' n times, and each resulting subnet m times */
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
		fprintf(stderr, "Too many splits required, aborting\n");
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
		subnet.mask = base_mask;
		for (k = 0; k < n_levels - 1; k++) {
			subnet.mask += mylog2(levels[k]);
			st_fprintf(out, "%N/%m;", subnet, subnet);
		}
		subnet.mask += mylog2(levels[k]); /* necessary to not print a final ; */
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
		fprintf(stderr, "Invalid level string '%s', starts with '%c'\n", s, s[0]);
		return -1;
	}
	while (1) {
		if (s[i] == '\0')
			break;
		if (s[i] == ',' && s[i + 1] == ',') {
			fprintf(stderr, "Invalid level string '%s', 2 consecutives ','\n", s);
			return -1;
		} else if (s[i] == ',' && s[i + 1] == '\0') {
                        fprintf(stderr, "Invalid level string '%s', ends with','\n", s);
                        return -1;
		}
		if (s[i] == ',') {
			levels[n_levels] = current;
			if (n_levels && current <= levels[n_levels - 1]) {
				fprintf(stderr, "Invalid split, %d >= %d\n", levels[n_levels - 1], current);
				return -1;
			}
			debug(SPLIT, 5, "split level#%d = %d\n", n_levels, levels[n_levels]);
			n_levels++;
			if (n_levels == 8) {
				fprintf(stderr, "Only 10 split levels are supported\n");
				return n_levels;
			}
			current = 0;
		} else if (isdigit(s[i])) {
			current *= 10;
			current += (s[i] - '0');
		} else {
			fprintf(stderr, "Invalid level string '%s', invalid char '%c'\n", s, s[i]);
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
		fprintf(stderr, "split mask lower than subnet mask\n");
		return -1;
	}
	n_levels = res;
	if  ((s->ip_ver == IPV4_A && levels[res - 1] > 32)
			|| (s->ip_ver == IPV6_A && levels[res - 1] > 128)
			|| (levels[n_levels - 1] - s->mask >=  sizeof(sum) * 8)) {
		fprintf(stderr, "Too many splits required, aborting\n");
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

/* sort a subnet file 'sf' with a custom cmp function
 *
 */
static int __subnet_sort_by(struct subnet_file *sf, int cmpfunc(void *v1, void *v2)) {
	unsigned long i;
	int res;
	TAS tas;
	struct route *new_r, *r;

	if (sf->nr == 0)
		return 0;
	debug_timing_start(2);
	res = alloc_tas(&tas, sf->nr, cmpfunc);
	if (res < 0) {
		debug_timing_end(2);
		return res;
	}
	new_r = malloc(sf->max_nr * sizeof(struct route));
	if (new_r == NULL) {
		fprintf(stderr, "%s : no memory\n", __FUNCTION__);
		free(tas.tab);
		debug_timing_end(2);
		return -1;
	}
	tas.print = __heap_print_subnet;
	debug(MEMORY, 3, "Allocated %lu bytes for new struct route\n", sf->max_nr * sizeof(struct route));
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

int fprint_routefilter_help(FILE *out) {

	return fprintf(out, "Routes can be filtered on :\n"
			" -prefix\n"
			" -mask\n"
			" -gw\n"
			" -device\n"
			" -comment\n"
			"\n"
			"operator are :\n"
			"- '=' (EQUALS)\n"
			"- '#' (DIFFERENT)\n"
			"- '<' (numerically inferior)\n"
			"- '>' (numerically superior)\n"
			"- '{' (is included (for prefixes))\n"
			"- '}' (includes (for prefixes))\n"
			"- '~' (st_scanf regular expression)\n");
}

/* filter a route 'object' against 'value' with operator 'op'
 *
 * s      : string to interpret
 * value  : its value
 * op     : the operator
 * object : a struct route; input string 's' will tell which field to test
 */
static int route_filter(char *s, char *value, char op, void *object) {
	struct route *route = object;
	struct subnet subnet;
	int res;

	debug(FILTER, 8, "Filtering '%s' %c '%s'\n", s, op, value);
	if (!strcmp(s, "prefix")) {
		res = get_subnet_or_ip(value, &subnet);
		if (res < 0) {
			debug(FILTER, 1, "Filtering on prefix %c '%s',  but it is not an IP\n", op, value);
			return -1;
		}
		res = subnet_compare(&route->subnet, &subnet);
		switch (op) {
		case '=':
			return (res == EQUALS);
			break;
		case '#':
			return !(res == EQUALS);
			break;
		case '<':
			return __heap_subnet_is_superior(&route->subnet, &subnet);
			break;
		case '>':
			return !__heap_subnet_is_superior(&route->subnet, &subnet) && res != EQUALS;
			break;
		case '{':
			return (res == INCLUDED || res == EQUALS);
			break;
		case '}':
			return (res == INCLUDES || res == EQUALS);
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for prefix\n", op);
			return -1;
		}
	}
	else if (!strcmp(s, "gw")) {
		if (route->gw.ip_ver == 0)
			return 0;
		res = get_subnet_or_ip(value, &subnet);
		if (res < 0) {
			debug(FILTER, 1, "Filtering on gw %c '%s',  but it is not an IP\n", op, value);
			return -1;
		}
		switch (op) {
		case '=':
			return is_equal_ip(&route->gw, &subnet.ip_addr);
			break;
		case '#':
			return !is_equal_ip(&route->gw, &subnet.ip_addr);
			break;
		case '<':
			return addr_is_superior(&route->gw, &subnet.ip_addr);
			break;
		case '>':
			return (!addr_is_superior(&route->gw, &subnet.ip_addr) &&
					!is_equal_ip(&route->gw, &subnet.ip_addr));
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for prefix\n", op);
			return -1;
		}
	}
	else if (!strcmp(s, "mask")) {
		res =  string2mask(value, 42);
		if (res < 0) {
			debug(FILTER, 1, "Filtering on mask %c '%s',  but it is valid\n", op, value);
			return -1;
		}
		switch (op) {
		case '=':
			return route->subnet.mask == res;
			break;
		case '#':
			return route->subnet.mask != res;
			break;
		case '<':
			return route->subnet.mask < res;
			break;
		case '>':
			return route->subnet.mask > res;
			break;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for mask\n", op);
			return 0;
		}
	}
	else if (!strcmp(s, "device")) {
		switch (op) {
		case '=':
			return !strcmp(route->device, value);
			break;
		case '#':
			return !!strcmp(route->device, value);
			break;
		case '~':
			res = st_sscanf(route->device, value);
			if (res == -1)
				return 0;
			return 1;
		default:
			debug(FILTER, 1, "Unsupported op '%c' for device\n", op);
			return -1;
		}
	}
	else if (!strcmp(s, "comment")) {
		switch (op) {
		case '=':
			return !strcmp(route->comment, value);
			break;
		case '#':
			return !!strcmp(route->comment, value);
			break;
		case '~':
			res = st_sscanf(route->comment, value);
			return (res < 0 ? 0 : 1);
		default:
			debug(FILTER, 1, "Unsupported op '%c' for comment\n", op);
			return -1;
		}
	}
	debug(FILTER, 1, "Cannot filter on attribute '%s'\n", s);
	return -1;
}

/*
 * filter a file with a regexp
 * sf   : the subnet file
 * expr : the pattern
 * returns :
 * 0  on SUCCESS
 * -1 on ERROR
 */
int subnet_filter(struct subnet_file *sf, char *expr) {
	int i, j, res, len;
	struct generic_expr e;
	struct route *new_r;

	if (sf->nr == 0)
		return 0;
	init_generic_expr(&e, expr, route_filter);
	debug_timing_start(2);

	new_r = malloc(sf->nr * sizeof(struct route));
	if (new_r == NULL) {
		fprintf(stderr, "%s : no memory\n", __FUNCTION__);
		debug_timing_end(2);
		return -1;
	}
	debug(MEMORY, 3, "Allocated %lu bytes for struct route\n", sf->nr * sizeof(struct route));
	j = 0;
	len = strlen(expr);

	for (i = 0; i < sf->nr; i++) {
		e.object = &sf->routes[i];
		res = run_generic_expr(expr, len, &e);
		if (res < 0) {
			fprintf(stderr, "Invalid filter '%s'\n", expr);
			free(new_r);
			debug_timing_end(2);
			return -1;
		}
		if (res) {
			st_debug(FILTER, 5, "Matching filter '%s' on %P\n", expr, sf->routes[i].subnet);
			copy_route(&new_r[j], &sf->routes[i]);
			j++;
		}
	}
	free(sf->routes);
	sf->routes = new_r;
	sf->nr = j;
	debug_timing_end(2);
	return 0;
}
