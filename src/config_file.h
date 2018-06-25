#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#define TYPE_STRING     1
#define TYPE_INT        2

#define CONFFILE_MAX_LINE_LEN	1024
#define CONFFILE_MAX_LEN	1024

struct file_options {
	char *name;
	int type; /* int or string */
	int size;
	size_t object_offset;
	/* offset in a  struct; don't set it manually,
	 * set it with offsetof(struct mystruct, my_field
	 */
	char *long_desc;
};


/* macro from linux kernel */
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)
#define sizeofmember(TYPE, MEMBER) sizeof(((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({ \
		const typeof(((type *)0)->member)*__mptr = (ptr); \
		(type *)((char *)__mptr - offsetof(type, member)); })

#define FILEOPT_LINE(__truc, __STRUCT, __TYPE)  \
	#__truc, __TYPE, sizeof(((__STRUCT *)0)->__truc), offsetof(__STRUCT, __truc)
/* This struct must be set in the *.c file using it*/
extern struct file_options fileoptions[];

int open_config_file(char *name, void *nof);
void config_file_describe(void);

#else
#endif

