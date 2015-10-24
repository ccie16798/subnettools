#ifndef ST_LIST_H
#define ST_LIST_H

/* macros taken from linux kernel */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define sizeofmember(TYPE, MEMBER) sizeof(((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({ \
		const typeof( ((type *)0)->member ) *__mptr = (ptr); \
		(type *)( (char *)__mptr - offsetof(type,member) );})

/* this struct should be embedded inside a struct XYZ
 * (except the list head, which MUST be initialized)
 *
 * struct mystruct {
 *	field a, b c;
 *	st_list list;
 * };
 * struct mystruct a, b, c, d, e, f;
 *
 * st_list head;
 * init_list(&head);
 *
 * list_add(&a.list, head);
 * list_add_tail(&b.list, head);
 * .....
 */
struct st_list {
        struct st_list *next;
	struct st_list *prev;
};

typedef struct st_list st_list;

#define LIST_FOR_EACH(__l, __head) \
	for (__l = (__head)->next; __l != (__head); __l = __l->next)

#define LIST_FOR_EACH_REVERSE(__l, __head) \
	for (__l = (__head)->prev; __l != (__head); __l = __l->prev)
#else
#endif
