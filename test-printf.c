#include <stdio.h>
#include <unistd.h>
#include "debug.h"
#include "iptools.h"
#include "st_printf.h"

int main(int argc, char **argv) {
	int 		a1 = 1000;
	unsigned 	a2 = 4000000000;
	int 		a3 = 4000000000;		
	unsigned short b = 65535;
	short c = 32767;
	short d = 32768;
	struct subnet s1, s2;
	char *string = "STRING";
	char buffer[64];

	get_subnet_or_ip("2.2.2.2", &s1);
	get_subnet_or_ip("2001:3:4::1", &s2);

	st_printf("%d %u %d\n", a1, a2, a3);
	st_printf("%06d %u %d\n", a1, a2, a3);
	st_printf("%s;%10s;%-10s\n", string, string, string);
	st_printf("%I-%10I\n", s1, s2);

}
