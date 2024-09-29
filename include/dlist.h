#include "common.h"

typedef struct dnode
{
    struct dnode *prev;
    struct dnode *next;
} DNODE;

typedef struct dlist
{
    DNODE *head;
		DNODE *tail;
} DLIST;

BOOL empty(DLIST *dlist);
void push_front(DLIST *dlist, DNODE *node);
void push_back(DLIST *dlist, DNODE *node);
DNODE* pop_front(DLIST *dlist);
DNODE* pop_back(DLIST *dlist);
void remove(DLIST *dlist, DNODE *node);
void insert_before(DLIST *dlist, DNODE *new_node, DNODE *node);
