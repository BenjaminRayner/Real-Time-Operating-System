#include "dlist.h"

typedef struct mailbox
{
    U8 *buf_start, *buf_end;
    U8 *head, *tail;
    size_t space;
    DLIST wait_list[4];
		DLIST rt_wait_list;
} MAILBOX;

BOOL mb_full(MAILBOX *mb);
BOOL mb_empty(MAILBOX *mb);

void enqueue(MAILBOX *mb, U8 data);
U8 dequeue(MAILBOX *mb);

int msg_len(MAILBOX *mb);
int total_size(MAILBOX *mb);
