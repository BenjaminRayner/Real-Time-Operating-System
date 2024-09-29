#include "k_mem.h"
#include "uart_irq.h"
#include "math.h"

BOOL mb_full(MAILBOX *mb)
{
  return mb->space == 0;
}
BOOL mb_empty(MAILBOX *mb)
{
  return (mb->tail == mb->head && mb->space != 0);
}

void enqueue(MAILBOX *mb, U8 data)
{
  *mb->tail = data;
	mb->tail++;
  mb->tail = (mb->tail == mb->buf_end) ? mb->buf_start : mb->tail;
  mb->space--;
}
U8 dequeue(MAILBOX *mb)
{
  U8 data = *mb->head;
	mb->head++;
  mb->head = (mb->head == mb->buf_end) ? mb->buf_start : mb->head;
  mb->space++;

  return data;
}

int msg_len(MAILBOX *mb)
{
	//If no wrap around
	if (mb->buf_end - mb->head >= 4) {
		return *(int *)mb->head;
	}
	
	U8 *head = mb->head;
	int len = 0;
	for (int i = 0; i < 4; ++i) {
		len += *head * upow(16, i);
		head++;
		head = (head == mb->buf_end) ? mb->buf_start : head;
	}
	
  return len;
}

int total_size(MAILBOX *mb)
{
	return mb->buf_end - mb->buf_start;
}
