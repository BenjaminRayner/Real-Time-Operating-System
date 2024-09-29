#include "dlist.h"

BOOL empty(DLIST *dlist)
{
	return dlist->head == NULL;
}

//Expecting that node is already placed in 
//its appropriate address in memory pool.
void push_front(DLIST *dlist, DNODE *node)
{
	node->next = NULL;
	node->prev = NULL;
	
	if (empty(dlist)) {
		dlist->tail = node;
	}
	else {
		node->next = dlist->head;
		dlist->head->prev = node;
	}
	dlist->head = node;
}
void push_back(DLIST *dlist, DNODE *node)
{
	node->next = NULL;
	node->prev = NULL;
	
	if (empty(dlist)) {
		dlist->head = node;
	}
	else {
		dlist->tail->next = node;
		node->prev = dlist->tail;
	}
	dlist->tail = node;
}

DNODE* pop_front(DLIST *dlist)
{
	if (empty(dlist)) {
		return NULL;
	}

	DNODE *temp = dlist->head;

	if (dlist->head->next != NULL) {
		dlist->head->next->prev = NULL;
	}
	else {
		dlist->tail = NULL;
	}
	dlist->head = dlist->head->next;

	temp->next = NULL;
	temp->prev = NULL;

	return temp;
}
DNODE* pop_back(DLIST *dlist)
{
	if (empty(dlist)) {
		return NULL;
	}

	DNODE *temp = dlist->tail;

	if (dlist->tail->prev != NULL) {
		dlist->tail->prev->next = NULL;
	}
	else {
		dlist->head = NULL;
	}
	dlist->tail = dlist->tail->prev;

	temp->next = NULL;
	temp->prev = NULL;

	return temp;
}

void remove(DLIST *dlist, DNODE *node)
{
	if (empty(dlist) || node == NULL) {
		return;
	}

	if (dlist->head == node) {
		pop_front(dlist);
	}
	else if (dlist->tail == node) {
		pop_back(dlist);
	}
	else {
		node->next->prev = node->prev;
		node->prev->next = node->next;
	}
	
	node->next = NULL;
	node->prev = NULL;
}

void insert_before(DLIST *dlist, DNODE *new_node, DNODE *node)
{
	if (dlist->head == node) {
		push_front(dlist, new_node);
	}
	else {
		new_node->prev = node->prev;
		node->prev->next = new_node;
		new_node->next = node;
		node->prev = new_node;
	}
}
