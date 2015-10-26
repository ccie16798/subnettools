/*
 * generic config file parsing implementation
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
#include "debug.h"
#include "utils.h"
#include "config_file.h"

int open_config_file(char *name, void *nof)
{
	FILE *f;
	char buffer[1024];
	char *s;
	unsigned long line = 0;
	int i, found, res;
	size_t offset;
	char *object;

	f = fopen(name, "r");
	if (f == NULL) {
		debug(CONFIGFILE, 2, "Cannot open config file '%s'\n", name);
		return -1;
	}
	object = nof;
	debug(CONFIGFILE, 3, "Opening config file '%s'\n", name);

	while ((s = fgets_truncate_buffer(buffer, sizeof(buffer), f, &i))) {
			line++;
			if (i) {
				debug(CONFIGFILE, 1, "%s line %lu is longer than max size %d\n",
						name, line, (int)sizeof(buffer));
			}
			s = strtok(s, " =\n");
			if (s == NULL) {
				debug(CONFIGFILE, 6, "%s line %lu is empty\n", name, line);
				continue;
			}
			if (s[0] == '#') {
				debug(CONFIGFILE, 5, "%s line %lu is a comment\n", name, line);
				continue;
			}
			found = 0;
			for (i = 0; ; i++) {
				if (fileoptions[i].name == NULL)
					break;
				if (!strcmp(s, fileoptions[i].name)) {
					found = 1;
					break;
				}
			}
			if (found == 0) {
				debug(CONFIGFILE, 2, "%s line %lu invalid config option : %s\n",
						name, line, s);
				continue;
			}
			debug(CONFIGFILE, 5, "%s line %lu valid config option found : %s\n",
					name, line, s);
			s = strtok(NULL, "\n");
			if (s == NULL) {
				debug(CONFIGFILE, 2, "%s line %lu no value found\n", name, line);
				continue;
			}

			offset = fileoptions[i].object_offset;
			switch (fileoptions[i].type) {
			case TYPE_STRING:
				debug(CONFIGFILE, 5, "line %lu copying STRING '%s'offset %d\n",
						line, s, (int)offset);
				res = strxcpy(object + offset, s,  fileoptions[i].size);
				if (res >= fileoptions[i].size) {
					debug(CONFIGFILE, 2, "line %lu STRING '%s' is too long\n",
							line, s);
					debug(CONFIGFILE, 2, "truncated to '%s'\n",
							object + offset);
				}
				break;
			case TYPE_INT:
				if (!isInt(s)) {
					debug(CONFIGFILE, 1, "line %lu expects an INT, got '%s'\n",
							line, s);
					break;
				}

				debug(CONFIGFILE, 5, "line %lu copying INT '%s' at offset %d\n",
						line, s, (int)offset);
				found = atoi(s);
				memcpy(object + offset, &found, fileoptions[i].size);
				break;
			default:
				debug(CONFIGFILE, 2, "line %lu unsupported type '%u'\n",
						line, fileoptions[i].type);
				break;
			}
	}
	fclose(f);
	return 0;
}

void config_file_describe(void)
{
	int i;
	char *s;

	for (i = 0; ; i++) {
		if (fileoptions[i].name == NULL)
			break;
		if (fileoptions[i].long_desc)
			s = fileoptions[i].long_desc;
		else
			s = "No available description";
		printf("%-20s : %s\n", fileoptions[i].name, s);
	}
}
#ifdef TEST_CONFIGFILE
int main(int argc, char **argv)
{
	struct st_options opt;
	debugs_level[__D_CONFIGFILE] = 6;
	open_config_file("st.conf", &opt);

}
#endif
