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

struct csvparser {
	const char *name;
	int (*parser)(char *name, FILE *, FILE *);
	const char *desc;
};

int cisco_route_to_csv(char *name, FILE *input_name, FILE *output);
int cisco_route_conf_to_csv(char *name, FILE *input_name, FILE *output);
int cisco_fw_to_csv(char *name, FILE *input_name, FILE *output);
int cisco_nexus_to_csv(char *name, FILE *input_name, FILE *output);
int ipso_route_to_csv(char *name, FILE *input_name, FILE *output);
void csvparser_help(FILE *output);

struct csvparser csvparsers[] = {
	{ "CiscoRouter", 	&cisco_route_to_csv, 	"ouput of 'show ip route' or 'sh ipv6 route' on Cisco IOS, IOS-XE" },
	{ "CiscoRouterConf", &cisco_route_conf_to_csv, "full configuration or ipv6/ipv4 static routes"},
	{ "CiscoFW", 	&cisco_fw_to_csv, 	"ouput of 'show route', IPv4 only"},
	{ "IPSO",		&ipso_route_to_csv, 	"output of clish show route"  },
	{ "GAIA",		&ipso_route_to_csv ,	"output of clish show route" },
	{ "CiscoNexus",	&cisco_nexus_to_csv ,	"output of show ip route on Cisco Nexus NXOS" },
	{ NULL, NULL }
};


void csvparser_help(FILE *output) {
	int i = 0;

	fprintf(output, "available parsers : \n");
	while (1) {
		if (csvparsers[i].name == NULL)
			break;
		fprintf(stderr, " %s : %s\n", csvparsers[i].name, csvparsers[i].desc);
		i++;
	}
}

int runcsv(char *name, char *filename, FILE *output) {
	int i = 0;
	FILE *f;

	if (!strcasecmp(name, "help")) {
		csvparser_help(stdout);
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
		if (csvparsers[i].name == NULL)
			break;
		if (!strcasecmp(name, csvparsers[i].name)) {
			csvparsers[i].parser(filename, f, output);
			return 0;
		}
		i++;

	}
	fprintf(stderr, "Unknow route parser : %s\n", name);
	csvparser_help(stderr);
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

/* Nexus route : GW is on another line
10.24.16.1/32, ubest/mbest: 1/0    ==> state->state[0] = 0
    *via 10.24.16.1, Vlan104, [0/0], 1y7w, hsrp     => state->state[0] = 1
*/
static int cisco_nexus_prefix_handle(char *s, void *data, struct csv_state *state) {
        struct  subnet_file *sf = data;
        int res;
        struct subnet subnet;

	if (state->state[0] == 0) { /* line should start with prefix */
		res = get_subnet_or_ip(s, &subnet);
		if (res == BAD_IP || res == IPV6_A || res == IPV4_A) {
			debug(PARSEROUTE, 3, "bad prefix '%s' line %lu\n", s, state->line);
			return CSV_INVALID_FIELD_BREAK;
		}
		if (state->state[1] == -1) /* state[1] == IP_VERSION, must be consistent */
			state->state[1] = subnet.ip_ver;
		else if (state->state[1] != subnet.ip_ver ) {
			debug(PARSEROUTE, 1, "line %lu inconsistent IP version, not supported\n", state->line);
			return CSV_INVALID_FIELD_BREAK;
		}

		memcpy(&sf->routes[sf->nr].subnet,  &subnet, sizeof(subnet));
		state->state[0] = 1;
		return CSV_VALID_FIELD_BREAK;
	} else { /* line should start with '*via' */
		if (strcasecmp(s, "*via")) {
			debug(PARSEROUTE, 2, "line %lu invalid, expected '*via' field but found '%s'\n", state->line, s);
			return CSV_INVALID_FIELD_BREAK;
		}
		return CSV_VALID_FIELD;
	}
}

static int cisco_nexus_gw_handle(char *s, void *data, struct csv_state *state)  {
        struct  subnet_file *sf = data;
        int res;
        struct subnet subnet;

	res = get_single_ip(s, &subnet);
	if (state->state[1] == IPV4_A && res == IPV4_A) {
		debug(PARSEROUTE, 5, "line %lu gw %s \n", state->line, s);
		memcpy(&sf->routes[sf->nr].gw6,  &subnet.ip6, sizeof(subnet.ip6));
		return CSV_VALID_FIELD;
	} else if  (state->state[1] == IPV6_A && res == IPV6_A)  {
		debug(PARSEROUTE, 5, "line %lu gw6 %s \n", state->line, s);
		memcpy(&sf->routes[sf->nr].gw6,  &subnet.ip6, sizeof(subnet.ip6));
		return CSV_VALID_FIELD;
	}  else if  (state->state[1] == IPV6_A && res  > 1000)  { /** for IPv6, there can be no NH or in case of MPBGP a NH in global table CHECK ME NEXUS**/
                        debug(PARSEROUTE, 5, "line %lu gw6 %s \n", state->line, s);
                        strxcpy(sf->routes[sf->nr].device, s, sizeof(sf->routes[sf->nr].device));
                        memset(&sf->routes[sf->nr].gw6, 0, 16);
			state->state[0] = 0;
			return CSV_VALID_FIELD_BREAK;
	} else {
		debug(PARSEROUTE, 2, "line %lu gw %s invalid IP\n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
}


static int cisco_nexus_device_handle(char *s, void *data, struct csv_state *state)  {
	struct  subnet_file *sf = data;

	if (s[0] == '[')  /* missing device, for example in a recursive route*/
		sf->routes[sf->nr].device[0] = '\0';
	else
		strxcpy(sf->routes[sf->nr].device, s, sizeof(sf->routes[sf->nr].device));
	debug(PARSEROUTE, 6, "line %lu found dev '%s'\n", state->line, s);
	state->state[0] = 0;
	return CSV_VALID_FIELD_BREAK;
}

static int noheader(char *s) {
	return 0;
}

int cisco_nexus_endofline_callback(struct csv_state *state, void *data) {
	struct  subnet_file *sf = data;
	if (state->badline) {
		state->state[0] = 0;
		debug(PARSEROUTE, 3, "Invalid line %lu\n", state->line);
		return -1;
	}
	if (state->state[0] == 0)
			sf->nr++;
	if  (sf->nr == sf->max_nr) {
		debug(MEMORY, 2, "need to reallocate memory\n");
		sf->max_nr *= 2;
		sf->routes = realloc(sf->routes,  sizeof(struct route) * sf->max_nr);
		if (sf->routes == NULL) {
			fprintf(stderr, "unable to reallocate, need to abort\n");
			return  CSV_CATASTROPHIC_FAILURE;
		}
	}
	return 1;
}

static int general_sf__endofline_callback(struct csv_state *state, void *data) {
        struct  subnet_file *sf = data;
	if (state->badline) {
                debug(PARSEROUTE, 2, "line %lu is bad \n", state->line);
		return -1;
	}
	sf->nr++;
        if  (sf->nr == sf->max_nr) {
                debug(MEMORY, 2, "need to reallocate memory\n");
                sf->max_nr *= 2;
                sf->routes = realloc(sf->routes,  sizeof(struct route) * sf->max_nr);
                if (sf->routes == NULL) {
                        fprintf(stderr, "unable to reallocate, need to abort\n");
                        return  CSV_CATASTROPHIC_FAILURE;
                }
        }
        return 1;
}

int cisco_nexus_to_csv(char *name, FILE *f, FILE *output) {
	struct csv_field csv_field[] = {
		{ "prefix"	, 0,  1, 0, &cisco_nexus_prefix_handle },
		{ "gw"		, 0,  2, 0, &cisco_nexus_gw_handle },
		{ "device"	, 0,  3, 0, &cisco_nexus_device_handle },
		{ NULL, 0,0,0, NULL }
	};
	struct csv_file cf;
	struct csv_state state;
	struct subnet_file sf;

	state.state[0] = 0; /*  == 0 ==> line with prefix, == 1 ==> line with GW/DEVICE */
	state.state[1] = -1; /*ip_ver */

	init_csv_file(&cf, csv_field, ", \n", &strtok_r);
	cf.is_header = &noheader;
	cf.endofline_callback = &cisco_nexus_endofline_callback;

	if (alloc_subnet_file(&sf, 4096) == -1)
		return -2;

	generic_load_csv(name, &cf, &state, &sf);
	fprintf(output, "prefix;mask;device;GW;comment\n");
	fprint_subnet_file(&sf, output, 2);
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
        if ( res == BAD_IP) {
                debug(PARSEROUTE, 2, "line %lu bad IP %s \n", state->line, s);
                return CSV_INVALID_FIELD_BREAK;
        }
        memcpy(&sf->routes[sf->nr], &subnet, sizeof(subnet));
        return CSV_VALID_FIELD;
}
static int ipso_gw_handle(char *s, void *data, struct csv_state *state) {
        struct  subnet_file *sf = data;
        int res;
        struct subnet subnet;

	if (state->state[0] == 1) /* connected route, s = "directly" */
		return CSV_VALID_FIELD;
	res = get_single_ip(s, &subnet);
        if ( res == BAD_IP) {
                debug(PARSEROUTE, 2, "line %lu bad GW %s \n", state->line, s);
                return CSV_INVALID_FIELD_BREAK;
        }
        memcpy(&sf->routes[sf->nr].gw6, &subnet.ip6, sizeof(subnet.ip6));
        return CSV_VALID_FIELD;
}
static int ipso_device_handle(char *s, void *data, struct csv_state *state) {
        struct  subnet_file *sf = data;

        strxcpy(sf->routes[sf->nr].device, s, sizeof(sf->routes[sf->nr].device));
	if (state->state[0] == 0) /* if non connected route, line is over ; break */
		return CSV_VALID_FIELD_BREAK;
        return CSV_VALID_FIELD;
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
			if ( res == IPV4_N) {
				subnet2str(&subnet, ip2, 2);
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
		} else if ( res == IPV4_A) { /* it is a line after x.X.X.X/mask is subnetted; so it doesnt have a mask */
			strcpy(ip2, s);;
			num_mask = find_mask;
			debug(PARSEROUTE, 5, "line %lu found IP %s without mask\n", line, s);
			s = strtok(NULL, "\n, ");
			debug(PARSEROUTE, 9, "line %lu parsing token %d : %s \n", line, token++, s);
		} else if ( res == IPV4_N) {
			subnet2str(&subnet, ip2, 2);
			num_mask = subnet.mask;
			debug(PARSEROUTE, 5, "line %lu found IP/mask %s\n", line, s);
			s = strtok(NULL, "\n, ");
			debug(PARSEROUTE, 9, "line %lu parsing token %d : %s \n", line, token++, s);
		} else if ( res == IPV6_N) {
			subnet2str(&subnet, ip2, 2);
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
		} else if ( res == IPV6_A) {
			debug(PARSEROUTE, 5, "line %lu found IPv6 %s without a mask, BUG\n", line, s);
			continue;
		}
		if (ip_ver == -1) {
			ip_ver = subnet.ip_ver % 10;
		} else if (ip_ver != subnet.ip_ver) {
			debug(PARSEROUTE, 1, "line %lu inconsistent IP version, not supported\n", line);
			continue;
		}

		while (s) { /** the gateway is after the 'via' keyword */
			if (s==NULL)
				break;
			if (!strcasecmp(s, "via")) {
				found_gw = 1;
				break;
			}
			s = strtok(NULL, "\n, ");
			token++;
		}
		if (found_gw==0 ) {
			debug(PARSEROUTE, 2, "line %lu no gw found for route\n", line);
			continue;
		}
		s = strtok(NULL, "\n, ");
		debug(PARSEROUTE, 9, "line %lu parsing token %d : %s \n", line, token++, s);
		if (s == NULL) {
			debug(PARSEROUTE, 2, "line %lu no GW after via field\n", line);
			continue;
		}

		res = get_single_ip(s, &subnet);
		if (ip_ver == IPV4_A && res == IPV4_A) {
			debug(PARSEROUTE, 5, "line %lu gw %s \n", line, s);
			gw = s;
		} else if  (ip_ver == IPV6_A && res == IPV6_A)  {
			debug(PARSEROUTE, 5, "line %lu gw6 %s \n", line, s);
			gw = s;
		} else if  (ip_ver == IPV6_A && res  > 1000)  { /** for IPv6, there can be no NH or in case of MPBGP a NH in global table **/
                        debug(PARSEROUTE, 5, "line %lu gw6 %s \n", line, s);
                        dev = s;
			gw = "";
			fprintf(output, "%s;%d;%s;%s;%s\n", ip2, num_mask, dev, gw, comment);
			continue;
                } else {
			debug(PARSEROUTE, 2, "line %lu gw %s invalid IP\n", line, s);
			gw = "";
		}
		s = strtok(NULL, "\n, ");
                debug(PARSEROUTE, 9, "line %lu parsing token %d : %s \n", line, token++, s);
                if (s == NULL) {
                        dev = "";
                } else {
			dev = s;
		}

		fprintf(output, "%s;%d;%s;%s;%s\n", ip2, num_mask, dev, gw, comment);

	} // while
	return 0;
}


static int cisco_fw_prefix_handle(char *s, void *data, struct csv_state *state) {
        struct  subnet_file *sf = data;
        int res;
        struct subnet subnet;

	res = get_single_ip(s, &subnet);
	if ( res == BAD_IP) {
		debug(PARSEROUTE, 2, "line %lu bad IP %s \n", state->line, s);
		return CSV_INVALID_FIELD_BREAK;
	}
	memcpy(&sf->routes[sf->nr], &subnet, sizeof(subnet));
	return CSV_VALID_FIELD;
}

static int cisco_fw_mask_handle(char *s, void *data, struct csv_state *state) {
        struct  subnet_file *sf = data;
        int num_mask;

	num_mask = string2mask(s);
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
	struct subnet subnet;
	int res;

	res = get_single_ip(s, &subnet);
        if (res == BAD_IP) {
                debug(PARSEROUTE, 2, "line %lu bad GW %s \n", state->line, s);
                return CSV_INVALID_FIELD_BREAK;
        }
	memcpy(&sf->routes[sf->nr].gw6, &subnet.ip6, sizeof(subnet.ip6));
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

int cisco_route_conf_to_csv(char *name, FILE *f, FILE *output) {
	int ip_ver = -1;
	char ip1[51];
	int num_mask;
	COMMON_PARSER_HEADER

	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		dev = "";
		comment = "";
		MOVE_TO_NEXT_TOKEN(s);
		if (res) {
			debug(PARSEROUTE, 1, "File %s line %lu is longer than max size %d, discarding %d chars\n",
					name, line, (int)sizeof(buffer), res);

		}
		if (!strcmp(s, "ip")) {
			if (ip_ver == -1)
				ip_ver = IPV4_A;
			else if (ip_ver != IPV4_A) {
				debug(PARSEROUTE, 1, "line %lu inconsistent IP version\n", line);
				continue;
			}
		} else if  (!strcmp(s, "ipv6")) {
			if (ip_ver == -1)
				ip_ver = IPV6_A;
			else if (ip_ver != IPV6_A) {
				debug(PARSEROUTE, 1, "line %lu inconsistent IP version\n", line);
				continue;
			}
		} else {
			debug(PARSEROUTE, 2, "line %lu doesnt start with ip/ipv6\n", line);
			continue;
		}
		MOVE_TO_NEXT_TOKEN(NULL);
		if (strcmp(s, "route")) {
			debug(PARSEROUTE, 2, "line %lu doesnt start with route\n", line);
			continue;
		}
		MOVE_TO_NEXT_TOKEN(NULL);
		if (!strcmp(s, "vrf")) { /** need to skip 2 tokens */
			debug(PARSEROUTE, 8, "vrf spotted line %lu\n", line);
			MOVE_TO_NEXT_TOKEN(NULL);
			MOVE_TO_NEXT_TOKEN(NULL);
		}
		res = get_single_ip(s, &subnet);
		if ( res == IPV4_A && ip_ver == IPV4_A ) {
			strcpy(ip1, s);
		} else if (res == IPV6_N && ip_ver == IPV6_A) {
			subnet2str(&subnet, ip1, 2);
			num_mask =  subnet.mask;
		} else {
			debug(PARSEROUTE, 2, "line %lu bad IP %s \n", line, s);
			continue;
		}
		if (ip_ver == IPV4_A ) { /* ipv6 got a mask in previous token */
			MOVE_TO_NEXT_TOKEN(NULL);
			num_mask = string2mask(s);
			if ( res == BAD_MASK) {
				debug(PARSEROUTE, 2, "line %lu bad mask %s \n", line, s);
				continue;
			}
		}
		MOVE_TO_NEXT_TOKEN(NULL);
		if (!isdigit(s[0])) { /* ip route 1.1.1.1 255.255.2555.0 Eth1/0 ... */
			dev =  s;
			MOVE_TO_NEXT_TOKEN(NULL);
		}
		res = get_single_ip(s, &subnet);
		if ( res == BAD_IP) {
			debug(PARSEROUTE, 2, "line %lu bad GW %s \n", line, s);
			continue;
		}
		gw = s;
		while (1) {
                        s = strtok(NULL, " \n");
                        if (s == NULL)
                                break;
                        if (!strcmp(s, "name")) {
                                s = strtok(NULL, " \n");
                                if ( s == NULL) {
                                        debug(PARSEROUTE, 2, "line %lu NULL Comment\n", line);
                                        break;
                                }
                                comment = s;
                        }
                } // while 1

		fprintf(output, "%s;%d;%s;%s;%s\n", ip1, num_mask, dev, gw, comment);
	} // while fgets
	return 0;
}
