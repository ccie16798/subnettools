/*
 * generic double linked-list manipulation functions
 * some parts are inspired from Linux Kernel by Linus Torvalds
 *
 * Copyright (C) 2015 Etienne Basset <etienne POINT basset AT ensta POINT org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include "st_list.h"


void init_list(st_list *list)
{
	list->next = list;
	list->prev = list;
}

int list_empty(st_list *list)
{
	return list == list->next;
}

int list_length(st_list *list)
{
	st_list *l;
	int i = 0;

	LIST_FOR_EACH(l, list)
		i++;
	return i;
}
/*
 * insert a nex entry after head
 */
void list_add(st_list *new, st_list *head)
{
	head->next->prev = new;
	new->next  = head->next;
	new->prev  = head;
	head->next = new;
}

/*
 * insert a nex entry at the end
 */
void list_add_tail(st_list *new, st_list *head)
{
	new->next = head;
	new->prev = head->prev;
	head->prev->next = new;
	head->prev = new;
}

void list_del(st_list *list)
{
	list->prev->next = list->next;
	list->next->prev = list->prev;
}

struct ab {
	int j;
	int k;
	st_list list;
	int a;
};

void print_list(st_list *head) {
	struct st_list *l;
	struct ab *a;

	LIST_FOR_EACH_REVERSE(l, head) {
		a = container_of(l, struct ab, list);
		printf("a=%d ", a->a);
	}
	printf("\n");
}

int main(int argc, char **argv)
{
	struct ab a, b, c,d;
	st_list list;
	init_list(&list);
	a.a = 1;
	list_add_tail(&a.list, &list);
	print_list(&list);
	b.a = 2;
	c.a = 3;
	d.a = 4;
	list_add_tail(&b.list, &list);
	print_list(&list);
	list_add_tail(&c.list, &list);
	print_list(&list);
	list_add_tail(&d.list, &list);
	print_list(&list);
	list_del(&b.list);
	print_list(&list);
	printf("len=%d\n", list_length(&list));
}
