/*
 * generic command line and options parser
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
#include "generic_command.h"
#include "debug.h"
#include "utils.h"
#include "st_memory.h"

#define MAX_AMBIGUOUS 39

static void enough_args(int argc, int value, char *com, char *progname) {
	if (argc < value ) {
		fprintf(stderr, "Not enough args for command '%s' \n", com);
		fprintf(stderr, "Please use '%s help' for more help\n", progname);
		exit(1);
	}
}

/* those struct MUST be filled in the main program */
extern struct st_command commands[];
extern struct st_command options[];

int generic_command_run(int argc, char **argv, char *progname, void *opt)
{
	int i, res;
	int found = 0, found_i = 0;
	int founds[MAX_AMBIGUOUS];

	debug(PARSEOPTS, 3, "parsing command : %s\n", argv[1]);
	for (i = 0; ;i++) {
		if (commands[i].name == NULL)
			break;
		if (!strncmp(commands[i].name, argv[1], strlen(argv[1]))) {
			if (strlen(argv[1]) == strlen(commands[i].name)) {
				found = 1;
				found_i = i;
				debug(PARSEOPTS, 5, "found EXACT handler for %s\n", argv[1]);
				break;
			}
			/* if more than one match, means the caller used an ambiguous abbreviation */
			if (found >= MAX_AMBIGUOUS - 1) /* enough is enough, OK??? */
				break;
			if (commands[i].hidden)
				continue;
			founds[found++] = i; 
			found_i = i;
			debug(PARSEOPTS, 5, "found possible handler %s for %s\n", commands[i].name, argv[1]);
		}
	}
	if (found == 0) {
		fprintf(stderr, "Unknow command '%s'\n", argv[1]);
		fprintf(stderr, "Please use '%s help' for more help\n", progname);
		res = -1;
	} else if (found >= 2) {
		fprintf(stderr, "Ambiguous command '%s', matches : \n", argv[1]);
		for (i = 0; i < found; i++)
			fprintf(stderr, " %s\n", commands[founds[i]].name);
		res = -2;
	} else if (found == 1) {
		enough_args(argc - 2,  commands[found_i].required_args, commands[found_i].name, progname);
		debug(PARSEOPTS, 3, "found handler for %s\n", argv[1]);
		debug_timing_start(1);
		res = commands[found_i].run_cmd(argc, argv, opt);
		debug_timing_end(1);
		debug(MEMORY, 4, "Total amout of memory still allocated %lu; %s\n", total_memory, argv[1]);
		if (total_memory != 0) {
			debug(MEMORY, 1, "Total amout of memory still allocated %lu; %si, memory leak?\n",
				 total_memory, argv[1]);
		}
	} else {
		debug(PARSEOPTS, 1, "BUG here '%s'\n", argv[1]);
		res = -1000;
	}
	return res;
}

/* take un-modified (int argc, char **argv) as arguments
 * returns negative if option is invalid or not enough argv
 * return an offset such as :
 *  new_argv = argv + offset
 *  new_argc = argc - offset
 */
int generic_parse_options(int argc, char **argv, char *progname, void *opt)
{
	int i, res, loop = 1;
	int a = 1;

	while (loop) {
		if (argv[a] == NULL) {
			if (a > 1)
				fprintf(stderr, "Missing command after options\n");
			else
				fprintf(stderr, "Missing command, use '%s help' for more help\n", progname);
			return -1;
		}
		debug(PARSEOPTS, 3, "parsing option : %s\n", argv[a]);
		for (i = 0; ;i++) {
			if (options[i].name == NULL) {
				loop = 0;
				break;
			}
			if (!strcmp(options[i].name, argv[a])) {
				debug(PARSEOPTS, 4, "found handler for %s\n", argv[a]);
				debug(PARSEOPTS, 6, "argc - a = %d, options[i].required_args = %d\n", argc - a , options[i].required_args);
				if (argc - a <= options[i].required_args) {
					fprintf(stderr, "Not enough arguments after option '%s'\n", argv[a]);
					return -1;
				}
				res = options[i].run_cmd(argc - a, argv + a, opt);
				if (res < 0)
					return res;
				a += (1 + options[i].required_args);
				break;
			}
		}
	}
	return a - 1;
}
