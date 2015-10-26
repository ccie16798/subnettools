#ifndef ST_LIST_H
#define ST_LIST_H

/* macros taken from linux kernel */
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)
#define sizeofmember(TYPE, MEMBER) sizeof(((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({ \
		const typeof(((type *)0)->member)*__mptr = (ptr); \
		(type *)((char *)__mptr - offsetof(type, member)); })

/*
 * struct st_list should be embedded inside a struct XYZ
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

void init_list(st_list *list);
int list_empty(st_list *list);
int list_length(st_list *list);
void list_add(st_list *new, st_list *head);
void list_add_tail(st_list *new, st_list *head);
void list_del(st_list *element);
void list_sort(st_list *head, int (*cmp)(st_list *, st_list *));

#define list_first_entry(__list_head, type, member) \
	container_of((__list_head)->next, type, member)

#define list_next_entry(pos, member) \
	container_of((pos)->member.next, typeof(*(pos)), member)

#define list_prev_entry(pos, member) \
	container_of((pos)->member.prev, typeof(*(pos)), member)

#define list_for_each(__l, __head) \
	for (__l = (__head)->next; __l != (__head); __l = __l->next)

#define list_for_each_reverse(__l, __head) \
	for (__l = (__head)->prev; __l != (__head); __l = __l->prev)

#define list_for_each_entry(pos, head, member) \
	for (pos = list_first_entry(head, typeof(*pos), member); \
			&pos->member != (head); \
			pos = list_next_entry(pos, member))
#else
#endif
