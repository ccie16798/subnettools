#ifndef ST_OBJECT
#define ST_OBJECT
#include "iptools.h"
/* subnet tools object */
struct sto { 
	char type;
	union {
		struct subnet 	s_subnet;
		struct ip_addr 	s_addr;
		int 		s_int;
		long 		s_long;
		char		s_char[64];
	};
};

#define consume_valist_from_object(__o, __n, __ap) do { \
	int __i; \
	void *ptr; \
	\
	for (__i = 0; __i < __n; __i++) { \
		debug(SCANF, 8, "restoring '%c' to va_list\n", __o[__i].type); \
		ptr = va_arg(__ap, void *); \
		switch (__o[__i].type) { \
			case '[': \
			case 's': \
			case 'W': \
			case 'S': \
				  strcpy((char *)ptr, __o[__i].s_char); \
			break; \
			case 'I': \
				  copy_ipaddr((struct ip_addr *)ptr, &__o[__i].s_addr); \
			break; \
			case 'P': \
				  copy_subnet((struct subnet *)ptr, &__o[__i].s_subnet); \
			break; \
			case 'd': \
			case 'M': \
				  *((int *)ptr) = __o[__i].s_int; \
			break; \
			case 'l': \
				  *((long *)ptr) = __o[__i].s_long; \
			break; \
			case 'c': \
				  *((char *)ptr) = __o[__i].s_char[0]; \
			break; \
			default: \
				 debug(SCANF, 1, "Unknown object type '%c'\n", __o[__i].type); \
		} \
	} \
	\
} while (0);

#else
#endif
