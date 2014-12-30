#ifndef ST_OBJECT
#define ST_OBJECT

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
#else
#endif
