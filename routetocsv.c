/*
 * 'sh ip route' to CSV converters
 *
 * Copyright (C) 2014, 2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
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
#include "bgp_tool.h"

struct csvconverter {
	const char *name;
	int (*converter)(char *name, FILE *, struct st_options *);
	const char *desc;
};

int cisco_route_to_csv(char *name, FILE *input_name, struct st_options *o);
int cisco_routeconf_to_csv(char *name, FILE *input_name, struct st_options *o);
int cisco_fw_conf_to_csv(char *name, FILE *input_name, struct st_options *o);
int cisco_fw_to_csv(char *name, FILE *input_name, struct st_options *o);
int cisco_nexus_to_csv(char *name, FILE *input_name, struct st_options *o);
int ipso_route_to_csv(char *name, FILE *input_name, struct st_options *o);
int palo_to_csv(char *name, FILE *input_name, struct st_options *o);
int ciscobgp_to_csv(char *name, FILE *input_name, struct st_options *o);
void csvconverter_help(FILE *output);

struct csvconverter csvconverters[] = {
	{ "CiscoRouter", 	&cisco_route_to_csv, 	"ouput of 'show ip route' or 'sh ipv6 route' on Cisco IOS, IOS-XE" },
	{ "CiscoRouterConf", 	&cisco_routeconf_to_csv,"full configuration or ipv6/ipv4 static routes" },
	{ "CiscoFWConf", 	&cisco_fw_conf_to_csv, 	"ouput of 'show conf' on ASA/PIX/FWSM"},
	{ "CiscoFW",	 	&cisco_fw_to_csv, 	"ouput of 'show route' one ASA/PIX/FWSM"},
	{ "IPSO",		&ipso_route_to_csv, 	"output of clish show route" },
	{ "GAIA",		&ipso_route_to_csv ,	"output of clish show route" },
	{ "CiscoNexus",		&cisco_nexus_to_csv ,	"output of show ip route on Cisco Nexus NXOS" },
	{ "palo",		&palo_to_csv ,		"output of show routing route on Palo Alto FW" },
	{ "ciscobgp",		&ciscobgp_to_csv ,	"output of show ip bgp on Cisco IOS" },
	{ NULL, NULL }
};

void csvconverter_help(FILE *output) {
	int i = 0;

	fprintf(output, "Available converters : \n");
	while (1) {
		if (csvconverters[i].name == NULL)
			break;
		fprintf(output, " %s : %s\n", csvconverters[i].name, csvconverters[i].desc);
		i++;
	}
}

/*
 * execute converter "name" on input file "filename"
 */
int run_csvconverter(char *name, char *filename, struct st_options *o) {
	int i = 0;
	FILE *f;

	if (!strcasecmp(name, "help")) {
		csvconverter_help(stdout);
		return 0;
	}
	if (filename == NULL) {
		fprintf(stderr, "Not enough arguments\n");
		return -1;
	}
	f = fopen(filename, "r");
	if (f == NULL) {
		fprintf(stderr, "Error: cannot open %s for reading\n", filename);
		return -2;
	}
	while (1) {
		if (csvconverters[i].name == NULL)
			break;
		if (!strcasecmp(name, csvconverters[i].name)) {
			csvconverters[i].converter(filename, f, o);
			return 0;
		}
		i++;
	}
	fprintf(stderr, "Unknow route converter : %s\n", name);
	csvconverter_help(stderr);
	return -2;
}
/*
 * output of 'show routing route' on Palo alto
 */
int palo_to_csv(char *name, FILE *f, struct st_options *o) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	int res;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");

        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		memset(&route, 0, sizeof(route));
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		res = st_sscanf(s, "%P *%I.*$%32s", &route.subnet, &route.gw, route.device);
		if (res < 1) {
			debug(PARSEROUTE, 9, "line %lu invalid route '%s'\n", line, buffer);
			badline++;
			continue;
		}
		/* on host route the last string is a flag; discard device in that case */
		if (strlen(route.device) < 3)
			route.device[0] = '\0';
		fprint_route(o->output_file, &route, 3);
	}
	return 1;
}
/*
 * output of 'show route' on IPSO or GAIA
 */
int ipso_route_to_csv(char *name, FILE *f, struct st_options *o) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	int res;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");

        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		memset(&route, 0, sizeof(route));
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		if (s[0] == 'C') {/* connected route */
			res = st_sscanf(s, ".*%Q.*$%32s", &route.subnet, route.device);
			if (res < 2) {
				debug(PARSEROUTE, 9, "line %lu invalid connected route '%s'\n", line, buffer);
				badline++;
				continue;
			}
			fprint_route(o->output_file, &route, 3);
			continue;
		}
		res = st_sscanf(s, ".*%Q *(via) %I.*%32[^ ,]", &route.subnet, &route.gw, route.device);
		if (res < 3) {
			debug(PARSEROUTE, 9, "line %lu invalid connected route '%s'\n", line, buffer);
			badline++;
			continue;
		}
		fprint_route(o->output_file, &route, 3);
	}
	return 1;
}

int cisco_nexus_to_csv(char *name, FILE *f, struct st_options *o) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	int res;
	int nhop = 0;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");

	memset(&route, 0, sizeof(route));
        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		if (strstr(s, "*via ")) {
			res = st_sscanf(s, " *(*via) %I.*%32[^ ,]", &route.gw, route.device);
			if (res == 0) {
				debug(PARSEROUTE, 4, "line %lu bad GW line no subnet found\n", line);
				badline++;
				continue;
			}
			if (res == 1)
				strcpy(route.device, "NA");
			if (route.device[0] == '[')
				strcpy(route.device, "NA");
			if (nhop == 0)
				fprint_route(o->output_file, &route, 3);
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
 * please take a coffee before reading
 */
int cisco_route_to_csv(char *name, FILE *f, struct st_options *o) {
        char buffer[1024];
        char *s;
        unsigned long line = 0;
        int badline = 0;
        struct route route;
        int res;
	int ip_ver = -1;
	int find_mask;
	int is_subnetted = 0;
	int find_hop = 0;
	char type;

	memset(&route, 0, sizeof(route));
	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");
#define CHECK_IP_VER  \
	if (ip_ver == -1) { \
		ip_ver = route.subnet.ip_ver; \
	} else if (route.subnet.ip_ver != ip_ver) { \
		debug(PARSEROUTE, 1, "line %lu Invalid '%s', inconsistent IP version\n", line, s); \
		badline++; \
		continue; \
	}
#define SET_COMMENT \
	if (o->rt) { \
		route.comment[0] = type; \
		route.comment[1] = ' '; \
		strxcpy(route.comment + 2, s + 2, 3); \
		remove_ending_space(route.comment); \
	}

	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);

		/* handle a next hop printed on a next-line
		 * happens in case the interface Name is long :)
		 */
		if (find_hop) {
			if (strstr(s, "via ")) {
				/* format is not the same in IPv6 or IPv4 */
				if (ip_ver == IPV4_A) {
				/*
				 O E1    10.150.10.128/25
					[110/45] via 10.138.2.131, 1w5d, TenGigabitEthernet8/1
				*/
					res = st_sscanf(s, ".*(via) (%I)?.*$%32[^, \n]", &route.gw, route.device);
				} else {
				/*
					S   2A02:8400:0:41::2:2/128 [1/0]
						     via FE80::253, Vlan491
				*/
					res = st_sscanf(s, " *(via) (%I)?.*%32[^, \n]", &route.gw, route.device);
				}
				if (res == 0) {
					debug(PARSEROUTE, 4, "line %lu bad GW line no subnet found\n", line);
					badline++;
					find_hop = 0;
					memset(&route, 0, sizeof(route));
					continue;
				}
				if (res == 1)
					strcpy(route.device, "NA");;
				if (route.device[0] == '[')
					strcpy(route.device, "NA");;
				fprint_route(o->output_file, &route, 3);
			} else {
				debug(PARSEROUTE, 4, "line %lu should hold a next-hop/device\n", line);
				badline++;
			}
			memset(&route, 0, sizeof(route));
			find_hop = 0;
			continue;
		}
		/* handle gateway of last resort line */
		if (!strncmp(s, "Gateway ", 8)) {
			ip_ver = IPV4_A;
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
			res = st_sscanf(s, ".*%P.*$%32s", &route.subnet, route.device);
			type = 'C';
			if (res < 2) {
				badline++;
				debug(PARSEROUTE, 1, "line %lu invalid connected route '%s'\n", line, s);
				memset(&route, 0, sizeof(route));
				continue;
			}
			CHECK_IP_VER
			SET_COMMENT
			fprint_route(o->output_file, &route, 3);
			memset(&route, 0, sizeof(route));
			continue;
		}
		/* a route after X.X.X.X/24 is subnetted doesnt hold a mask */
		if (is_subnetted) {
			res = st_sscanf(s, "%c *%I.*(via) %I.*$%32S", &type, &route.subnet.ip_addr, &route.gw, route.device);
			if (res < 3) {
				badline++;
				debug(PARSEROUTE, 1, "line %lu invalid 'is subnetted' '%s'\n", line, s);
				is_subnetted = 0;
				memset(&route, 0, sizeof(route));
				continue;
			}
			if (res == 3) {
				debug(PARSEROUTE, 4, "line %lu no device found\n", line);
				strcpy(route.device, "NA");
			}
			CHECK_IP_VER
			route.subnet.mask = find_mask;
			is_subnetted--;
			if (isdigit(route.device[0]))
				strcpy(route.device, "NA");
			SET_COMMENT
			fprint_route(o->output_file, &route, 3);
			memset(&route, 0, sizeof(route));
			continue;
		}
		res = st_sscanf(s, "%c *.*%P.*(via) %I.*$%32s", &type, &route.subnet, &route.gw, route.device);
		/* a valid route begin with a non space char */
		if (res <= 1 || isspace(type)) {
			badline++;
			debug(PARSEROUTE, 1, "line %lu Invalid line '%s'\n", line, s);
			memset(&route, 0, sizeof(route));
			continue;
		} else if (res == 2) { /* in case next-hop appears on next line */
			find_hop = 1;
			SET_COMMENT
			continue;
		} else  if (res == 3) {
			debug(PARSEROUTE, 4, "line %lu no device found\n", line);
			strcpy(route.device, "NA");
		}
		find_hop = 0;
		CHECK_IP_VER
		if (isdigit(route.device[0]))
			strcpy(route.device, "NA");
		SET_COMMENT

		fprint_route(o->output_file, &route, 3);
		memset(&route, 0, sizeof(route));
	}
	return 1;
}
/*
 * input from ASA firewall or FWSM
 **/
int cisco_fw_to_csv(char *name, FILE *f, struct st_options *o) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	int res;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");

	memset(&route, 0, sizeof(route));
        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		res = st_sscanf(s, "(ipv6 )?route *%32S *%I.%M %I", route.device, &route.subnet.ip_addr, &route.subnet.mask, &route.gw);
		if (res < 4) {
			debug(PARSEROUTE, 2, "Invalid line %lu\n", line);
			badline++;
			memset(&route, 0, sizeof(route));
			continue;
		}

	}
	return 1;
}
/*
 * input from ASA firewall or FWSM
 **/
int cisco_fw_conf_to_csv(char *name, FILE *f, struct st_options *o) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	int res;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");

	memset(&route, 0, sizeof(route));
        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		res = st_sscanf(s, "(ipv6 )?route *%32S *%I.%M %I", route.device, &route.subnet.ip_addr, &route.subnet.mask, &route.gw);
		if (res < 4) {
			debug(PARSEROUTE, 2, "Invalid line %lu\n", line);
			badline++;
			memset(&route, 0, sizeof(route));
			continue;
		}
		fprint_route(o->output_file, &route, 3);
		memset(&route, 0, sizeof(route));
	}
	return 1;
}

int cisco_routeconf_to_csv(char *name, FILE *f, struct st_options *o) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	struct sto sto[10];
	int res;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");
	memset(&route, 0, sizeof(route));
	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer : '%s'", line, s);
		res = sto_sscanf(buffer, "ip(v6)? route.*%I.%M (%32S)? *%I.*(name) %128s", sto, 6);
		if (res < 2) {
			debug(PARSEROUTE, 2, "Invalid line %lu\n", line);
			badline++;
			continue;
		}
		copy_ipaddr(&route.subnet.ip_addr, &sto[0].s_addr);
		route.subnet.mask = sto[1].s_int;
		if (sto_is_string(&sto[2]))
			strcpy(route.device, sto[2].s_char);
		if (res >= 4 && sto[3].type == 'I')
			copy_ipaddr(&route.gw, &sto[3].s_addr);
		if (res >= 5 && sto[4].type == 's')
			strcpy(route.comment, sto[4].s_char);
		fprint_route(o->output_file, &route, 3);
		memset(&route, 0, sizeof(route));
		sto[1].type = sto[2].type = sto[3].type = sto[4].type = 0;
	}
	return 1;
}


int ciscobgp_to_csv(char *name, FILE *f, struct st_options *o) {
	char buffer[1024];
	char *s, *s2;
	unsigned long line = 0;
	int badline = 0;
	struct bgp_route route;
	struct subnet last_subnet;
	int res;
	int med_offset = 34, aspath_offset = 61;

	fprintf(o->output_file, "V;Proto;BEST;          prefix;              GW;       MED;LOCAL_PREF;    WEIGHT;AS_PATH;\n");
	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		memset(&route, 0, sizeof(route));
		debug(PARSEROUTE, 9, "line %lu buffer : '%s'", line, s);

		s2 = strstr(s, "Metric");
		if (s2) {
			med_offset = s2 - s - strlen("Metric") + 1;
			s2 = strstr(s, "Path");
			if (s2 == NULL) {
				debug(PARSEROUTE, 1, "Invalid Header line, missing Path keyword %lu\n", line);
				badline++;
				continue;
			}
			aspath_offset = s2 - s;
			debug(PARSEROUTE, 3, "med_offset %d, aspath_offset %d\n", med_offset, aspath_offset);
		}
		res = st_sscanf(s, ".*%P *%I", &route.subnet, &route.gw);
		if (res == 0) {
			debug(PARSEROUTE, 2, "Invalid line %lu\n", line);
			badline++;
			continue;
		}
		if (s[0] == '*')
			route.valid = 1;
		if (s[1] == '>')
			route.best = 1;
		if (s[2] == 'i')
			route.type = 'i';
		else
			route.type = 'e';
		if (res == 1) {/* prefix was on last line */
			copy_ipaddr(&route.gw, &route.subnet.ip_addr);
			copy_subnet(&route.subnet, &last_subnet);
		} else
			copy_subnet(&last_subnet, &route.subnet);
		res = st_sscanf(s + med_offset, " {1,10}(%d)? {1,6}(%d)? {1,6}(%d)?", &route.MED,
					 &route.LOCAL_PREF, &route.weight);
		res = st_sscanf(s + aspath_offset, "%[0-9: ]", &route.AS_PATH);
		fprint_bgp_route(o->output_file, &route);
	}
	return 1;
}
