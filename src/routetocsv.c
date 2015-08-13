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
	{ "CiscoFWConf", 	&cisco_fw_conf_to_csv, 	"ouput of 'show conf' (IPv4/IPv6) on ASA/PIX/FWSM"},
	{ "CiscoFW",	 	&cisco_fw_to_csv, 	"ouput of 'show route' (IPv4) one ASA/PIX/FWSM"},
	{ "IPSO",		&ipso_route_to_csv, 	"output of clish show route" },
	{ "GAIA",		&ipso_route_to_csv ,	"output of clish show route" },
	{ "CiscoNexus",		&cisco_nexus_to_csv ,	"output of show ip route/show ipv6 route on Cisco Nexus NXOS" },
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

#define BAD_LINE_CONTINUE \
	debug(PARSEROUTE, 4, "%s line %lu invalid route : '%s'", name, line, buffer); \
	memset(&route, 0, sizeof(route)); \
	badline++; \
	continue; \

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

/*
 * output of 'show routing route' on Palo alto
 */
int palo_to_csv(char *name, FILE *f, struct st_options *o) {
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	int ip_ver = -1;
	int res;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");

        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		memset(&route, 0, sizeof(route));
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		res = st_sscanf(s, "%P *%I.*$%32s", &route.subnet, &route.gw, route.device);
		if (res < 1) {
			BAD_LINE_CONTINUE
		}
		CHECK_IP_VER
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
	int nhop = 0;
	int ip_ver = -1;
	char type;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");

        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		if (isspace(s[0])) /* strangely some lines are prepended with a space ....*/
			s++;
		if (s[0] == 'C') {/* connected route */
			memset(&route, 0, sizeof(route));
			res = st_sscanf(s, ".*%Q.*$%32s", &route.subnet, route.device);
			if (res < 2) {
				BAD_LINE_CONTINUE
			}
			CHECK_IP_VER
			type = 'C';
			SET_COMMENT
			fprint_route(o->output_file, &route, 3);
			continue;
		}
		if (isspace(s[0])) {
			if (strstr(s, "via ")) {
				res = st_sscanf(s, ".*(via) %I.*%32[^ ,]", &route.gw, route.device);
				if (res < 2) {
					BAD_LINE_CONTINUE
				}
			} else {
				BAD_LINE_CONTINUE
			}
			if (nhop == 0 || o->ecmp)
				fprint_route(o->output_file, &route, 3);
			nhop++;
			continue;
		}
		nhop = 1;
		memset(&route, 0, sizeof(route));
		res = st_sscanf(s, ".*%Q *(via) %I.*%32[^ ,]", &route.subnet, &route.gw, route.device);
		type = s[0];
		if (res < 3) {
			BAD_LINE_CONTINUE
		}
		CHECK_IP_VER
		SET_COMMENT
		fprint_route(o->output_file, &route, 3);
	}
	return 1;
}

int cisco_nexus_to_csv(char *name, FILE *f, struct st_options *o) {
	char buffer[1024];
	char poubelle[128];
	char *s;
	unsigned long line = 0;
	int badline = 0;
	struct route route;
	int res;
	int nhop = 0;
	int ip_ver = -1;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");

	memset(&route, 0, sizeof(route));
        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		if (strstr(s, "*via ")) {
/*	    *via 128.90.8.22, Vlan35, [170/512256], 2w0d, eigrp-WAN, external, tag 1387965159 */
/*   *via 128.90.8.34, [1/0], 11w0d, static, tag 65159
 *
 *   THAT pattern is fun
 */

			res = st_sscanf(s, " *(*via) %I(, %[][0-9/]%s|, %[^,], %[^,],).*, %[^,]",
					 &route.gw, route.device, poubelle, route.comment);
			if (res == 0) {
				BAD_LINE_CONTINUE
			}
			if (res == 1)
				strcpy(route.device, "NA");
			if (route.device[0] == '[') /* route without a device */
				strcpy(route.device, "NA");
			if (o->rt == 0)
				route.comment[0] = '\0';
			if (nhop == 0 || o->ecmp)
				fprint_route(o->output_file, &route, 3);
			CHECK_IP_VER
			nhop++;
		} else {
			memset(&route, 0, sizeof(route));
			res = st_sscanf(s, "%P", &route.subnet);
			if (res == 0) {
				BAD_LINE_CONTINUE
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

	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);

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
		} else if (strstr(s, "is directly connected")) {
			memset(&route, 0, sizeof(route));
			/* C       10.73.5.92/30 is directly connected, Vlan346 */
			res = st_sscanf(s, ".*%P.*$%32s", &route.subnet, route.device);
			type = 'C';
			if (res < 2) {
				BAD_LINE_CONTINUE
			}
			CHECK_IP_VER
			SET_COMMENT
			fprint_route(o->output_file, &route, 3);
			memset(&route, 0, sizeof(route));
			continue;
		}
		/* handle a next hop printed on a next-line
		 * happens in case the interface Name is long :)
		 */
		if (isspace(s[0])) {
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
					find_hop = 0;
					BAD_LINE_CONTINUE
				}
				if (res == 1)
					strcpy(route.device, "NA");;
				if (route.device[0] == '[')
					strcpy(route.device, "NA");;
				if (is_subnetted) {
					route.subnet.mask = find_mask;
					is_subnetted--;
				}
			} else {
				find_hop = 0;
				BAD_LINE_CONTINUE
			}
			if (find_hop == 1 || o->ecmp)
				fprint_route(o->output_file, &route, 3);
			find_hop++;
			continue;
		}
		memset(&route, 0, sizeof(route));
		res = st_sscanf(s, "%c *.*%P.*(via) %I.*$%32s", &type, &route.subnet, &route.gw, route.device);
		/* a valid route begin with a non space char */
		if (res <= 1 || isspace(type)) {
			BAD_LINE_CONTINUE
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
		if (is_subnetted) {
			route.subnet.mask = find_mask;
			is_subnetted--;
		}
		if (isdigit(route.device[0]))
			strcpy(route.device, "NA");
		SET_COMMENT
		fprint_route(o->output_file, &route, 3);
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
	char type;
	int find_hop = 0;
	int ip_ver = -1;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");

	memset(&route, 0, sizeof(route));
        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		if (find_hop) {
			res = st_sscanf(s, ".*(via )%I.*$%32s", &route.gw, route.device);
			if (res < 2) {
				BAD_LINE_CONTINUE
			}
			SET_COMMENT
			fprint_route(o->output_file, &route, 3);
			memset(&route, 0, sizeof(route));
			find_hop = 0;
			continue;
		}
		if (s[0] == 'C' || s[0] == 'c') { /* connected route */
			res = st_sscanf(s, "%c.*%I %M.*$%32s", &type, &route.subnet.ip_addr, &route.subnet.mask, route.device);
			if (res < 4) {
				BAD_LINE_CONTINUE
			}
		} else {
			res = st_sscanf(s, "%c.*%I %M.*(via )%I.*$%32s", &type, &route.subnet.ip_addr, &route.subnet.mask, &route.gw, route.device);
			if (res == 3) {
				find_hop = 1;
				continue;
			}
			if (res < 5) {
				BAD_LINE_CONTINUE
			}
		}
		CHECK_IP_VER
		SET_COMMENT
		fprint_route(o->output_file, &route, 3);
		memset(&route, 0, sizeof(route));
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
	int ip_ver = -1;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");

	memset(&route, 0, sizeof(route));
        while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer '%s'\n", line, buffer);
		res = st_sscanf(s, "(ipv6 )?route *%32S *%I.%M %I", route.device, &route.subnet.ip_addr, &route.subnet.mask, &route.gw);
		if (res < 4) {
			BAD_LINE_CONTINUE
		}
		CHECK_IP_VER
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
	int ip_ver = -1;

	fprintf(o->output_file, "prefix;mask;device;GW;comment\n");
	memset(&route, 0, sizeof(route));
	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		debug(PARSEROUTE, 9, "line %lu buffer : '%s'", line, s);
		res = sto_sscanf(buffer, "ip(v6)? route.*%I.%M (%32S)? *%I.*(name) %128s", sto, 6);
		if (res < 2) {
			BAD_LINE_CONTINUE
		}
		copy_ipaddr(&route.subnet.ip_addr, &sto[0].s_addr);
		route.subnet.mask = sto[1].s_int;
		CHECK_IP_VER
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

	fprint_bgp_file_header(o->output_file);
	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &res))) {
		line++;
		memset(&route, 0, sizeof(route));
		debug(PARSEROUTE, 9, "line %lu buffer : '%s'", line, s);

		s2 = strstr(s, "Metric");
		if (s2) {
			med_offset = s2 - s - strlen("Metric") + 1;
			s2 = strstr(s, "Path");
			if (s2 == NULL) {
				debug(PARSEROUTE, 1, "Invalid Header line %lu, missing Path keyword\n", line);
				badline++;
				continue;
			}
			aspath_offset = s2 - s;
			debug(PARSEROUTE, 3, "med_offset %d, aspath_offset %d\n", med_offset, aspath_offset);
		}
		res = st_sscanf(s, ".*%Q *%I", &route.subnet, &route.gw);
		if (res == 0) {
			debug(PARSEROUTE, 2, "Invalid line %lu\n", line);
			badline++;
			continue;
		}
		if (s[0] == '*')
			route.valid = 1;
		if (s[1] == '>')
			route.best = 1;
		if (s[2] == 'i') /* FIXME */
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
		if (res != 3) {
				debug(PARSEROUTE, 1, "Line %lu Invalid, no MED or WEIGHT or LOCAL_PREF\n", line);
				badline++;
				continue;
		}
		res = st_sscanf(s + aspath_offset, "(%256[0-9: ])?%c", &route.AS_PATH, &route.origin);
		if (res != 2) {
				debug(PARSEROUTE, 1, "Line %lu Invalid, no ASP_PATH/ORIGIN\n", line);
				badline++;
				continue;
		}
		remove_ending_space(route.AS_PATH);
		fprint_bgp_route(o->output_file, &route);
	}
	return 1;
}
