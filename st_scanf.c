#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "utils.h"
#include "iptools.h"
#include "debug.h"

static int st_vscanf(char *in, const char *fmt, va_list ap) {
	int i, j;
	int n_found;
	char c;
	struct subnet v_sub;
	int v_int;
	int v_long;
	
	i = 0; /* index in fmt */
	j = 0; /* j index in in */
	n_found = 0; /* number of argument found */
	while (1) {
		c = fmt[i];
		if (c == '\0') {
			return n_found;
		} else if (c == '%') {



		} else if (c == '\\') {
			switch (fmt[i + 1]) {
				case 't':
					c = '\t';
					break;
				case ' ':
					c = ' ';
					break;
				case '?':
					c = '?';
					break;
				case '.':
					c = '.';
					break;
				default:
					break;
			}
		} else if (c == '?') {
			debug(SCANF, 2, "fmt[%d]='?', match any char\n", i);
			i++;
			j++;
		} else {
			if (in[j] != fmt[i]) {
				debug(SCANF, 2, "in[%d]=%c, != fmt[%d]=%c, exiting\n",
					j, in[j], i, fmt[i]);
				return  n_found;
			}
			i++;
			j++;
		}

	}



}

int st_sscanf(char *in, const char *fmt, ...) {
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = st_vscanf(in, fmt, ap);
	va_end(ap);
	return ret;
}

