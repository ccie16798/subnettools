/*
 * 'sh ip route' to CSV converters
 *
 * Copyright (C) 2014 Etienne Basset <etienne POINT basset AT ensta POINT org>
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
#include "routetocsv.h"
#include "iptools.h"
#include "generic_csv.h"
#include "utils.h"
#include "st_printf.h"
#include "st_scanf.h"

struct csvconverter {
	const char *name;
	int (*converter)(char *name, FILE *, FILE *);
	const char *desc;
};

int cisco_route_to_csv(char *name, FILE *input_name, FILE *output);
int cisco_route_conf_to_csv(char *name, FILE *input_name, FILE *output);
int cisco_fw_to_csv(char *name, FILE *input_name, FILE *output);
int cisco_fw_conf_to_csv(char *name, FILE *input_name, FILE *output);
int cisco_nexus_to_csv(char *name, FILE *input_name, FILE *output);
int ipso_route_to_csv(char *name, FILE *input_name, FILE *output);
void csvconverter_help(FILE *output);

struct csvconverter csvconverters[] = {
	{ "CiscoRouter", 	&cisco_route_to_csv, 	"ouput of 'show ip route' or 'sh ipv6 route' on Cisco IOS, IOS-XE" },
	{ "CiscoRouterConf", &cisco_route_conf_to_csv, "full configuration or ipv6/ipv4 static routes"},
	{ "CiscoFWConf", 	&cisco_fw_conf_to_csv, 	"ouput of 'show conf', IPv4 only"},
	{ "IPSO",		&ipso_route_to_csv, 	"output of clish show route"  },
	{ "GAIA",		&ipso_route_to_csv ,	"output of clish show route" },
	{ "CiscoNexus",	&cisco_nexus_to_csv ,	"output of show ip route on Cisco Nexus NXOS" },
	{ NULL, NULL }
};


void csvconverter_help(FILE *output) {
	int i = 0;

	fprintf(output, "available converters : \n");
	while (1) {
		if (csvconverters[i].name == NULL)
			break;
		fprintf(stderr, " %s : %s\n", csvconverters[i].name, csvconverters[i].desc);
		i++;
	}
}

int run_csvconverter(char *name, char *filename, FILE *output) {
	int i = 0;
	FILE *f;

	if (!strcasecmp(name, "help")) {
		csvconverter_help(stdout);
		return 0;
	}
	if (filename == NULL) {
		fprintf(stderr, "not enough arguments\n");
		return -1;
	}
	f = fopen(filename, "r");
	if (f == NULL) {
		fprintf(stderr, "error: cannot open %s for reading\n", filename);
		return -2;
	}
	while (1) {
		if (csvconverters[i].name == NULL)
			break;
		if (!strcasecmp(name, csvconverters[i].name)) {
			csvconverters[i].converter(filename, f, output);
			return 0;
		}
		i++;

	}
	fprintf(stderr, "Unknow route converter : %s\n", name);
	csvconverter_help(stderr);
	return -2;
}
/* those fucking classfull IPSO nokia will print 10/8, 172.18/16 */
int classfull_get_subnet(const char *string, struct subnet *subnet) {
	char s2[51], out[51];
	char *s, *save_s;
	int truc[4];

	strxcpy(s2, string,sizeof(s2));
	memset(truc, 0, sizeof(truc));
	s = s2;
	s = strtok_r(s, "/\n", &save_s);
	if (s == NULL) {
		debug(PARSEIP,2, "Invalid prefix nokia %s\n", string);
		return BAD_IP;
	}
	sscanf(s, "%d.%d.%d.%d", &truc[0], &truc[1], &truc[2], &truc[3]);
	s = strtok_r(NULL, "/\n", &save_s);
	if (s == NULL) {
		debug(PARSEIP,2, "Invalid prefix nokia %s, no mask\n", string);
		return BAD_IP;
	}
	sprintf(out, "%d.%d.%d.%d/%s", truc[0], truc[1], truc[2], truc[3], s);
	return get_subnet_or_ip(out, subnet);
}

#define MOVE_TO_NEXT_TOKEN(__x) \
	do { \
		s = strtok(__x, " \n"); \
		debug(PARSEROUTE, 9, "line %lu parsing token %d : %s \n", line, token, s); \
		if (s==NULL) { \
			debug(PARSEROUTE, 5, "line %lu is invalid, only %d tokens \n", line, token); \
			badline = 1; \
			break; \
		}\
		token++; \
	} while (0); \
if ( badline ) \
	continue;
#define COMMON_PARSER_HEADER \
	char buffer[1024]; \
	char *s; \
	unsigned long line = 0; \
	int badline = 0; \
	struct subnet subnet; \
	int token; \
	int res; \
	char *gw, *dev="", *comment=""; \
	fprintf(output, "prefix;mask;device;GW;comment\n");



int cisco_nexus_to_csv(char *name, FILE *f, FILE *output) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	struct sto o[10];
	int res;
	int search_prefix = 1;

	fprintf(output, "prefix;mask;device;GW;comment\n");

	memset(&route, 0, sizeof(route));
        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		if (search_prefix) {
			res = st_sscanf(s, "%P", &route.subnet);
			if (res == 0) {
				debug(PARSEROUTE, 4, "line %lu bad IP no subnet found\n", line);
				badline++;
				continue;
			}
			search_prefix = 0;
		} else {
			res = sto_sscanf(s, " *(*via) %I.*%[^ ,]", o, 4);
			if (res == 0) {
				debug(PARSEROUTE, 4, "line %lu bad GW line no subnet found\n", line);
				badline++;
				continue;
			}
			if (o[0].s_addr.ip_ver == route.subnet.ip_ver) {
				copy_ipaddr(&route.gw, &o[0].s_addr);
			} else {
				st_debug(PARSEROUTE, 4, "line %lu bad GW '%I'\n", line, o[0].s_addr);	
			}
			if (o[1].s_char[0] != '\0' && o[1].s_char[0] != '[') {
				strxcpy(route.device, o[1].s_char, sizeof(route.device));
			} else {
				st_debug(PARSEROUTE, 4, "line %lu void device'\n", line);	
			}
			fprint_route(&route, output, 3);
			search_prefix = 1;
			memset(&route, 0, sizeof(route));
		}

	}
	return 1;

}

static int ipso_type_handle(char *s, void *data, struct csv_state *state) {
	if (s[0] == 'C' )
		state->state[0] = 1; /* connected route */
	else
		state->state[0] = 0;
	return CSV_VALID_FIELD;
}

static int ipso_prefix_handle(char *s, void *data, struct csv_state *state) {
	struct  subnet_file *sf = data;
	int res;
	struct subnet subnet;

	res = classfull_get_subnet(s, &subnet);
	if (res == BAD_IP) {
		debug(PARSEROUTE, 2, "line %lu bad IP %s \n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	copy_subnet(&sf->routes[sf->nr].subnet, &subnet);
	return CSV_VALID_FIELD;
}

static int ipso_gw_handle(char *s, void *data, struct csv_state *state) {
	struct  subnet_file *sf = data;
	int res;
	struct ip_addr addr;

	if (state->state[0] == 1) /* connected route, s = "directly" */
		return CSV_VALID_FIELD;
	res = get_single_ip(s, &addr, 41);
	if ( res == BAD_IP) {
		debug(PARSEROUTE, 2, "line %lu bad GW %s \n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	copy_ipaddr(&sf->routes[sf->nr].gw, &addr);
	return CSV_VALID_FIELD;
}

static int ipso_device_handle(char *s, void *data, struct csv_state *state) {
	struct  subnet_file *sf = data;

	strxcpy(sf->routes[sf->nr].device, s, sizeof(sf->routes[sf->nr].device));
	if (state->state[0] == 0) /* if non connected route, line is over ; break */
		return CSV_VALID_FIELD_BREAK;
	return CSV_VALID_FIELD;
}

static int general_sf__endofline_callback(struct csv_state *state, void *data) {
	struct subnet_file *sf = data;
	if (state->badline) {
		debug(PARSEROUTE, 2, "line %lu is bad \n", state->line);
		return -1;
	}
	sf->nr++;
	if (sf->nr == sf->max_nr) {
		debug(MEMORY, 2, "need to reallocate memory\n");
		sf->max_nr *= 2;
		sf->routes = realloc(sf->routes, sizeof(struct route) * sf->max_nr);
		if (sf->routes == NULL) {
			fprintf(stderr, "unable to reallocate, need to abort\n");
			return CSV_CATASTROPHIC_FAILURE;
		}
	}
	return 1;
}

static int noheader(char *s) {
	return 0;
}
/* ouput of clish gaia, ipso */
int ipso_route_to_csv(char *name, FILE *f, FILE *output) {
	struct csv_field csv_field[] = {
		{ "type"	, 0,  1, 0, &ipso_type_handle },
		{ "prefix"	, 0,  2, 0, &ipso_prefix_handle },
		{ "gw"		, 0,  4, 0, &ipso_gw_handle },
		{ "device"	, 0,  5, 0, &ipso_device_handle },
		{ "device2"	, 0,  6, 0, &ipso_device_handle }, /* device is on 6th field in case of connected route */
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;
	struct subnet_file sf;

	state.state[0] = 0; /*  == 0 ==> line with prefix, == 1 ==> line with GW/DEVICE */
	state.state[1] = -1; /*ip_ver */

	init_csv_file(&cf, csv_field, ", \n", &strtok_r);
	cf.is_header = &noheader;
	cf.endofline_callback = &general_sf__endofline_callback;

	if (alloc_subnet_file(&sf, 4096) == -1)
		return -2;

	generic_load_csv(name, &cf, &state, &sf);
	fprintf(output, "prefix;mask;device;GW;comment\n");
	fprint_subnet_file(&sf, output, 2);
	return 1;
}


/*
 * output of show ip route or show ipv6 route
 * cisco IOS, IOS-XE
 * cant be converted to generic_csv because too many special cases (wtf ? X.X.X.X is subnetted, ....)
 */
int cisco_route_to_csv(char *name, FILE *f, FILE *output) {
	int find_mask = -1;
	int found_gw;
	int num_mask;
	char ip2[51];
	int ip_ver = -1;
	struct ip_addr addr;
	char *s2;
	COMMON_PARSER_HEADER

	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		dev="";
		num_mask=0;
		found_gw = 0;
		token=1;
		badline = 0;
		if (res) {
			debug(PARSEROUTE, 1, "File %s line %lu is longer than max size %d, discarding %d chars\n",
					name, line, (int)sizeof(buffer), res);
		}
		debug(PARSEROUTE, 9, "parsing line %lu : %s\n", line, s);
		if (strstr(s, "variably subnetted")!=NULL ) {
			debug(PARSEROUTE, 5, "line %lu \'is variably subnetted\', ignoring\n", line);
			continue;
		} else if (strstr(s, "is subnetted")) {
			debug(PARSEROUTE, 5, "line %lu \'is subnetted'\n", line);
			MOVE_TO_NEXT_TOKEN(s);
			res = get_subnet_or_ip(s, &subnet);
			if ( res > 1000 ) {
				debug(PARSEROUTE, 2, "line %lu bad IP %s \n", line, s);
				continue;
			}
			find_mask = subnet.mask;
			continue;
		} else if (strstr(s,"is directly connected")) {
			char *previous_s;
			debug(PARSEROUTE, 5, "line %lu connected route \n", line);
			MOVE_TO_NEXT_TOKEN(s);
			MOVE_TO_NEXT_TOKEN(NULL);
			res = get_subnet_or_ip(s, &subnet);
			if (res == IPV4_N) {
				subnet2str(&subnet, ip2, sizeof(ip2), 2);
				num_mask = subnet.mask;
				gw = "0.0.0.0";
			} else if (res == IPV6_N) {
				/** in fact this should not happen */
				gw = "::";
			} else {
				debug(PARSEROUTE, 2, "line %lu invalid connected IP %s \n", line, s);
				continue;
			}
			if (ip_ver == -1) { /** checking the IP version is same as the first one we found **/
				ip_ver = subnet.ip_ver % 10;
			} else if (ip_ver != subnet.ip_ver) {
				debug(PARSEROUTE, 1, "line %lu inconsistent IP version, not supported\n", line);
				continue;
			}
			while (1) { /* go to last token */
				previous_s = s;
				s = strtok(NULL, " \n");
				if (s == NULL)
					break;
			}
			dev = previous_s;
			fprintf(output, "%s;%d;%s;%s;%s\n", ip2, num_mask, dev, gw, comment);
			continue;
		}

		MOVE_TO_NEXT_TOKEN(s); /** type of route (part 1) **/
		MOVE_TO_NEXT_TOKEN(NULL); /** type of route (part 2) or IP address**/

		/** now, there could be an IP adress **/
		res = get_subnet_or_ip(s, &subnet);
		if (res > 1000) {
			debug(PARSEROUTE, 6, "line %lu %s is not IP, probably route type\n", line, s);
			MOVE_TO_NEXT_TOKEN(NULL); /** IP address**/
			res = get_subnet_or_ip(s, &subnet);
		}
		if (res > 1000) {
			debug(PARSEROUTE, 5, "line %lu %s is not IP, probably route type\n", line, s);
			continue;
		} else if (res == IPV4_A) { /* it is a line after x.X.X.X/mask is subnetted; so it doesnt have a mask */
			strcpy(ip2, s);;
			num_mask = find_mask;
			debug(PARSEROUTE, 5, "line %lu found IP %s without mask\n", line, s);
			s = strtok(NULL, "\n, ");
			debug(PARSEROUTE, 9, "line %lu parsing token %d : %s \n", line, token++, s);
		} else if (res == IPV4_N) {
			subnet2str(&subnet, ip2, sizeof(ip2), 2);
			num_mask = subnet.mask;
			debug(PARSEROUTE, 5, "line %lu found IP/mask %s\n", line, s);
			s = strtok(NULL, "\n, ");
			debug(PARSEROUTE, 9, "line %lu parsing token %d : %s \n", line, token++, s);
		} else if (res == IPV6_N) {
			subnet2str(&subnet, ip2, sizeof(ip2), 2);
			num_mask = subnet.mask;
			debug(PARSEROUTE, 5, "line %lu found IPv6/mask %s\n", line, s);
			/** in show ipv6 route, the NH is printed on the next line so get a new line **/
			s = fgets(buffer, sizeof(buffer), f);
			line++;
			token = 1;
			if (s == NULL) {
				debug(PARSEROUTE, 3, "no line %lu but there should be one, cannot get a next hop\n", line);
				continue;
			}
			s = strtok(s, "\n, ");
			debug(PARSEROUTE, 9, "line %lu parsing token %d : %s \n", line, token++, s);
		} else if (res == IPV6_A) {
			debug(PARSEROUTE, 5, "line %lu found IPv6 %s without a mask, BUG\n", line, s);
			continue;
		}
		if (ip_ver == -1) {
			ip_ver = subnet.ip_ver;
		} else if (ip_ver != subnet.ip_ver) {
			debug(PARSEROUTE, 1, "line %lu inconsistent IP version, not supported\n", line);
			continue;
		}

		while (s) { /** the gateway is after the 'via' keyword */
			if (s == NULL)
				break;
			if (!strcasecmp(s, "via")) {
				found_gw = 1;
				break;
			}
			s = strtok(NULL, "\n, ");
			token++;
		}
		if (found_gw == 0) {
			debug(PARSEROUTE, 2, "line %lu no gw found for route\n", line);
				continue;
			}
		s = strtok(NULL, "\n, ");
		debug(PARSEROUTE, 9, "line %lu parsing token %d : %s \n", line, token++, s);
		if (s == NULL) {
			debug(PARSEROUTE, 2, "line %lu no GW after via field\n", line);
			continue;
		}

		res = get_single_ip(s, &addr, 41);
		if (ip_ver == IPV4_A && res == IPV4_A) {
			debug(PARSEROUTE, 5, "line %lu gw %s \n", line, s);
			gw = s;
		} else if (ip_ver == IPV6_A && res == IPV6_A) {
			debug(PARSEROUTE, 5, "line %lu gw6 %s \n", line, s);
			gw = s;
		} else if (ip_ver == IPV6_A && res  > 1000) { /** for IPv6, there can be no NH or in case of MPBGP a NH in global table **/
			debug(PARSEROUTE, 5, "line %lu gw6 %s \n", line, s);
			dev = s;
			gw = "";
			fprintf(output, "%s;%d;%s;%s;%s\n", ip2, num_mask, dev, gw, comment);
			continue;
		} else {
			debug(PARSEROUTE, 2, "line %lu gw %s invalid IP\n", line, s);
			gw = "";
		}
		/* trying to find the device, if any
		   device will be the last token */
		s = strtok(NULL, "\n, ");
		s2 = NULL;
		while (s) {
			s2 = s;
			s = strtok(NULL, "\n, ");
			debug(PARSEROUTE, 9, "line %lu parsing token %d : %s \n", line, token++, s);
		}
		if (s2)
			dev = s2;
		else
			dev = "";
		fprintf(output, "%s;%d;%s;%s;%s\n", ip2, num_mask, dev, gw, comment);

	} // while
	return 0;
}


static int cisco_fw_prefix_handle(char *s, void *data, struct csv_state *state) {
	struct  subnet_file *sf = data;
	int res;
	struct ip_addr addr;

	res = get_single_ip(s, &addr, 41);
	if (res == BAD_IP) {
		debug(PARSEROUTE, 2, "line %lu bad IP %s \n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	copy_ipaddr(&sf->routes[sf->nr].subnet.ip_addr, &addr);
	//memcpy(&sf->routes[sf->nr], &subnet, sizeof(subnet));
	return CSV_VALID_FIELD;
}

static int cisco_fw_mask_handle(char *s, void *data, struct csv_state *state) {
	struct  subnet_file *sf = data;
	int num_mask;

	num_mask = string2mask(s, 21);
	if ( num_mask == BAD_MASK) {
		debug(PARSEROUTE, 2, "line %lu bad mask %s \n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	sf->routes[sf->nr].subnet.mask = num_mask;
	return CSV_VALID_FIELD;
}

static int cisco_fw_route_handle(char *s, void *data, struct csv_state *state) {
	if (strcmp(s, "route")) {
		debug(PARSEROUTE, 5, "line %lu doesnt start with route\n", state->line); /* debug 5 becoz maybe the file doesnt contain only routes */
		return CSV_INVALID_FIELD_BREAK;
	} else
		return CSV_VALID_FIELD;
}

static int cisco_fw_dev_handle(char *s, void *data, struct csv_state *state) {
	struct  subnet_file *sf = data;

	strxcpy(sf->routes[sf->nr].device, s, sizeof(sf->routes[sf->nr].device));
	return CSV_VALID_FIELD;
}

static int cisco_fw_gw_handle(char *s, void *data, struct csv_state *state) {
	struct  subnet_file *sf = data;
	struct ip_addr addr;
	int res;

	res = get_single_ip(s, &addr, 41);
	if (res == BAD_IP) {
		debug(PARSEROUTE, 2, "line %lu bad GW %s \n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	copy_ipaddr(&sf->routes[sf->nr].gw, &addr);
	return CSV_VALID_FIELD;
}

int cisco_fw_to_csv(char *name, FILE *f, FILE *output) {
	struct csv_field csv_field[] = {
		{ "route"	, 0,  1, 0, &cisco_fw_route_handle },
		{ "device"	, 0,  2, 0, &cisco_fw_dev_handle },
		{ "prefix"	, 0,  3, 0, &cisco_fw_prefix_handle },
		{ "mask"	, 0,  4, 0, &cisco_fw_mask_handle },
		{ "gw"		, 0,  5, 0, &cisco_fw_gw_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;
	struct subnet_file sf;

	state.state[1] = -1; /*ip_ver */

	init_csv_file(&cf, csv_field, ", \n", &strtok_r);
	cf.is_header = &noheader;
	cf.endofline_callback = &general_sf__endofline_callback;

	if (alloc_subnet_file(&sf, 4096) == -1)
		return -2;

	generic_load_csv(name, &cf, &state, &sf);
	fprintf(output, "prefix;mask;device;GW;comment\n");
	fprint_subnet_file(&sf, output, 2);
	return 1;
}

int cisco_fw_conf_to_csv(char *name, FILE *f, FILE *output) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	int res;

	fprintf(output, "prefix;mask;device;GW;comment\n");

	memset(&route, 0, sizeof(route));
        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		res = st_sscanf(s, "(ipv6 )?route *%S *%I.%M %I", route.device, &route.subnet.ip_addr, &route.subnet.mask, &route.gw);
		if (res < 4) {
			debug(PARSEROUTE, 2, "Invalid line %lu\n", line);
			badline++;
			continue;
		}
		fprint_route(&route, output, 3);
		memset(&route, 0, sizeof(route));
	}
	return 1;
}

int cisco_route_conf_to_csv(char *name, FILE *f, FILE *output) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	struct sto o[10];
	int res;

	fprintf(output, "prefix;mask;device;GW;comment\n");
	memset(&route, 0, sizeof(route));
	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer : '%s'", line, s);
		res = sto_sscanf(buffer, "ip(v6)? route.*%I.%M (%S)? *%I.*(name) %s", o, 6);
		if (res < 2) {
			debug(PARSEROUTE, 2, "Invalid line %lu\n", line);
			badline++;
			continue;
		}
		copy_ipaddr(&route.subnet.ip_addr, &o[0].s_addr);
		route.subnet.mask = o[1].s_int;
		if (sto_is_string(&o[2]))
			strcpy(route.device, o[2].s_char);
		if (res >= 4 && o[3].type == 'I')
			copy_ipaddr(&route.gw, &o[3].s_addr);
		if (res >= 5 && o[4].type == 's')
			strcpy(route.comment, o[4].s_char);
		fprint_route(&route, output, 3);
		memset(&route, 0, sizeof(route));
		o[1].type = o[2].type = o[3].type = o[4].type = 0;
	}
	return 1;
}
