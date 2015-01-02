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
	{ "CiscoNexus",		&cisco_nexus_to_csv ,	"output of show ip route on Cisco Nexus NXOS" },
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
/*
 * output of 'show route' on IPSO or GAIA
 */
int ipso_route_to_csv(char *name, FILE *f, FILE *output) {
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
		if (s[0] == 'C') {/* connected route */
			res = st_sscanf(s, ".*%Q.*$%s", &route.subnet, route.device);
			if (res < 2) {
				debug(PARSEROUTE, 9, "line %lu invalid connected route '%s'\n", line, buffer);
				badline++;
				continue;
			}
			fprint_route(&route, output, 3);
			memset(&route, 0, sizeof(route));
		}
		res = st_sscanf(s, ".*%Q *(via) %I.*%[^ ,]", &route.subnet, &route.gw, route.device);
		if (res < 3) {
			debug(PARSEROUTE, 9, "line %lu incorret connected route '%s'\n", line, buffer);
			badline++;
			continue;
		}
		fprint_route(&route, output, 3);
		memset(&route, 0, sizeof(route));

	}
	return 1;
}

int cisco_nexus_to_csv(char *name, FILE *f, FILE *output) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	int res;
	int nhop = 0;

	fprintf(output, "prefix;mask;device;GW;comment\n");

	memset(&route, 0, sizeof(route));
        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		if (strstr(s, "*via ")) {
			res = st_sscanf(s, " *(*via) %I.*%[^ ,]", &route.gw, route.device);
			if (res == 0) {
				debug(PARSEROUTE, 4, "line %lu bad GW line no subnet found\n", line);
				badline++;
				continue;
			}
			if (res == 1)
				route.device[0] = '\0';
			if (route.device[0] == '[')
				route.device[0] = '\0';
			if (nhop == 0)
				fprint_route(&route, output, 3);
			nhop++;
		} else {
			memset(&route, 0, sizeof(route));
			res = st_sscanf(s, "%P", &route.subnet);
			if (res == 0) {
				debug(PARSEROUTE, 4, "line %lu bad IP no subnet found\n", line);
				badline++;
				continue;
			}
			nhop = 0;
		}

	}
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
#define CHECK_IP_VER  \
	if (ip_ver == -1) { \
		ip_ver = route.subnet.ip_ver; \
	} else if (route.subnet.ip_ver != ip_ver) { \
		debug(PARSEROUTE, 1, "line %lu Invalid '%s', inconsistent IP version\n", line, s); \
		badline++; \
		continue; \
	} \

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
			CHECK_IP_VER
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
				memset(&route, 0, sizeof(route));
				continue;
			}
			if (res == 2) {
				debug(PARSEROUTE, 4, "line %lu no device found\n", line);
				route.device[0] = '\0';
			}
			CHECK_IP_VER
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
		CHECK_IP_VER
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
