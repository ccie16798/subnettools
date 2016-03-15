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

static inline void init_list(st_list *list)
{
	list->next = list;
	list->prev = list;
}

static inline int list_empty(st_list *list)
{
	return list == list->next;
}

int list_length(st_list *list);

/* list_add: add an element to a list
 * @new  : the element to add
 * @head : the existing list head
 */
static inline void list_add(st_list *new, st_list *head)
{
	new->next  = head->next;
	new->prev  = head;
	head->next->prev = new;
	head->next = new;
}

/* list_add_tail: add an element to the end of a list
 * @new  : the element to add
 * @head : the existing list head
 */
static inline void list_add_tail(st_list *new, st_list *head)
{
	new->next = head;
	new->prev = head->prev;
	head->prev->next = new;
	head->prev = new;
}

/* list_del: remove an element from the list it belongs to
 * @list : the element to remove
 */
static inline void list_del(st_list *list)
{
	list->prev->next = list->next;
	list->next->prev = list->prev;
}

/* list elements are added between head and head->next */
static inline void __list_join(st_list *list, st_list *head)
{
	st_list *first = list->next;
	st_list *last  = list->prev;

	last->next = head->next;
	head->next->prev = last;

	first->prev = head;
	head->next  = first;
}

/* list_join: add list 'list' to the beginning of list 'head'
 * @list : the list to be added
 * @head : the list
 */
static inline void list_join(st_list *list, st_list *head)
{
	if (list_empty(list))
		return;
	__list_join(list, head);
}

/* list elements are added to head */
static inline void __list_join_tail(st_list *list, st_list *head)
{
	st_list *first = list->next;
	st_list *last  = list->prev;

	first->prev = head->prev;
	head->prev->next = first;

	last->next = head;
	head->prev = last;
}

/* list_join_tail: add list 'list' to the end of list 'head'
 * @list : the list to be added
 * @head : the list
 */
static inline void list_join_tail(st_list *list, st_list *head)
{
	if (list_empty(list))
		return;
	__list_join_tail(list, head);
}

/* list_sort: sort a list according to a cmp func
 * @head : the list head
 * @cmd  : a compare func that returns:
 *  > 0 if list1 is better than l2
 *  < 0 if list1 is worse  than l2
 *  0   if list1 is equal to l2
 */
void list_sort(st_list *head, int (*cmp)(st_list *l1, st_list *l2));

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
