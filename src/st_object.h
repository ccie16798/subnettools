#ifndef ST_OBJECT
#define ST_OBJECT

#include "iptools.h"
#include "st_options.h"

/* subnet tools object */
struct sto {
	char type;
	char conversion; /* like 'l' for long int */
	union {
		struct subnet	s_subnet;
		struct ip_addr	s_addr;
		int		s_int;
		short		s_short;
		unsigned short	s_ushort;
		unsigned int	s_uint;
		long		s_long;
		unsigned long	s_ulong;
		char		s_char[ST_OBJECT_MAX_STRING_LEN + 1]; /* +1 for NUL */
	};
};

int sto_is_string(struct sto *o);

int sto2string(char *s, const struct sto *o, size_t len, int comp_level);

#define consume_valist_from_object(__o, __n, __ap) \
do { \
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
		case 'Q': \
			copy_subnet((struct subnet *)ptr, &__o[__i].s_subnet); \
			break; \
		case 'x': \
			if (__o[__i].conversion  == 'l') \
				*((unsigned long *)ptr) = __o[__i].s_ulong; \
			else if (__o[__i].conversion  == 'h') \
				*((unsigned short *)ptr) = __o[__i].s_ushort; \
			else \
				*((unsigned int *)ptr) = __o[__i].s_uint; \
			break; \
		case 'u': \
			if (__o[__i].conversion  == 'l') \
				*((unsigned long *)ptr) = __o[__i].s_ulong; \
			else if (__o[__i].conversion  == 'h') \
				*((unsigned short *)ptr) = __o[__i].s_ushort; \
			else \
				*((unsigned int *)ptr) = __o[__i].s_uint; \
			break; \
		case 'd': \
			if (__o[__i].conversion  == 'l') \
				*((long *)ptr) = __o[__i].s_long; \
			else if (__o[__i].conversion  == 'h') \
				*((short *)ptr) = __o[__i].s_short; \
			else \
				*((int *)ptr) = __o[__i].s_int; \
			break; \
		case 'M': \
			*((int *)ptr) = __o[__i].s_int; \
			break; \
		case 'l': \
			*((long *)ptr) = __o[__i].s_long; \
			break; \
		case 'c': \
			*((char *)ptr) = __o[__i].s_char[0]; \
			break; \
		case '\0': \
			break; \
		default: \
			debug(SCANF, 1, "Unknown object type '%c'\n", __o[__i].type); \
			break; \
		} \
	} \
} while (0)

#else
#endif
