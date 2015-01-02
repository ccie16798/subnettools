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
int cisco_fw_conf_to_csv(char *name, FILE *input_name, FILE *output);
int cisco_nexus_to_csv(char *name, FILE *input_name, FILE *output);
int ipso_route_to_csv(char *name, FILE *input_name, FILE *output);
void csvconverter_help(FILE *output);

struct csvconverter csvconverters[] = {
	{ "CiscoRouter", 	&cisco_route_to_csv, 	"ouput of 'show ip route' or 'sh ipv6 route' on Cisco IOS, IOS-XE" },
	{ "CiscoRouterConf", 	&cisco_route_conf_to_csv,"full configuration or ipv6/ipv4 static routes"},
	{ "CiscoFWConf", 	&cisco_fw_conf_to_csv, 	"ouput of 'show conf'"},
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
 */
int cisco_route_to_csv(char *name, FILE *f, FILE *output) {
        char buffer[1024];
        char *s;
        unsigned long line = 0;
        int badline = 0;
        struct route route;
        int res;
	int ip_ver = -1;
	int find_mask;
	int is_subnetted = 0;

	memset(&route, 0, sizeof(route));
	fprintf(output, "prefix;mask;device;GW;comment\n");
	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		if (!strncmp(s, "Gateway ", 8)) {
                        debug(PARSEROUTE, 5, "line %lu \'is gateway of last resort, skipping'\n", line);
			continue;
		} else if (strstr(s, "variably subnetted")) {
                        debug(PARSEROUTE, 5, "line %lu \'is variably subnetted, skipping'\n", line);
			continue;
		} else if (strstr(s, "is subnetted")) {
			debug(PARSEROUTE, 5, "line %lu \'is subnetted'\n", line);
			find_mask = route.subnet.mask;
			/*      194.51.71.0/32 is subnetted, 1 subnets */
			res = st_sscanf(s, " *%I/%d is subnetted, %d subnets", &route.subnet.ip_addr, &find_mask, &is_subnetted);
			if (res < 3) {
				badline++;
				debug(PARSEROUTE, 1, "line %lu invalid 'is subnetted' '%s'\n", line, s);
				continue;
			}
			continue;
		} else if (strstr(s, "directly connected")) {
			/* C       10.73.5.92/30 is directly connected, Vlan346 */
			res = st_sscanf(s, ".*%P.*$%s", &route.subnet, route.device);
			if (res < 2) {
				badline++;
				debug(PARSEROUTE, 1, "line %lu invalid connected route '%s'\n", line, s);
				memset(&route, 0, sizeof(route));
				continue;
			}
			if (ip_ver == -1) {
				ip_ver = route.subnet.ip_ver;
			} else if (route.subnet.ip_ver != ip_ver) {
				debug(PARSEROUTE, 1, "line %lu Invalid '%s', inconsistent IP version\n", line, s);
				badline++;
				continue;
			}
			fprint_route(&route, output, 3);
			memset(&route, 0, sizeof(route));
			continue;
		}
		if (is_subnetted) {
			res = st_sscanf(s, ".*%I.*(via) %I.*$%S", &route.subnet.ip_addr, &route.gw, route.device);
			if (res < 2) {
				badline++;
				debug(PARSEROUTE, 1, "line %lu invalid 'is subnetted' '%s'\n", line, s);
				is_subnetted = 0;
				continue;
			}
			if (res == 2) {
				debug(PARSEROUTE, 1, "line %lu no device found\n", line);
				route.device[0] = '\0';	
			}
			if (ip_ver == -1) {
				ip_ver = route.subnet.ip_ver;
			} else if (route.subnet.ip_ver != ip_ver) {
				debug(PARSEROUTE, 1, "line %lu Invalid '%s', inconsistent IP version\n", line, s);
				badline++;
				continue;
			}
			route.subnet.mask = find_mask;
			is_subnetted--;
			fprint_route(&route, output, 3);
			memset(&route, 0, sizeof(route));
			continue;
		}
		res = st_sscanf(s, ".*%P.*(via) %I.*$%s", &route.subnet, &route.gw, route.device);
		if (res == 0) {
			badline++;
			debug(PARSEROUTE, 1, "line %lu Invalid line '%s'\n", line, s);
			memset(&route, 0, sizeof(route));
			continue;
		}
		if (ip_ver == -1) {
			ip_ver = route.subnet.ip_ver;
		} else if (route.subnet.ip_ver != ip_ver) {
			debug(PARSEROUTE, 1, "line %lu Invalid '%s', inconsistent IP version\n", line, s);
			badline++;
			continue;
		}
		if (ip_ver == IPV6_A) {
			s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res);
			line++;
			if (s == NULL) {
				debug(PARSEROUTE, 1, "line %lu Invalid line, no next hop found\n", line);
				break;
			}
			res = st_sscanf(s, " *(via) (%I)?.*%[^, \n]", &route.gw, route.device);
			if (res == 0) {
				debug(PARSEROUTE, 1, "line %lu Invalid line '%s', no next-hop\n", line, s);
				continue;
			}
		} else {
			if (ip_ver == -1)
				ip_ver = IPV6_A;
			if (res < 2) {
				debug(PARSEROUTE, 1, "line %lu Invalid line '%s', no next-hop\n", line, s);
				continue;
			}
		}
		fprint_route(&route, output, 3);
		memset(&route, 0, sizeof(route));
	}
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
