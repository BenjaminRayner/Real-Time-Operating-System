/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTX LAB  
 *
 *                     Copyright 2020-2022 Yiqing Huang
 *                          All rights reserved.
 *---------------------------------------------------------------------------
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *---------------------------------------------------------------------------*/
 

/**************************************************************************//**
 * @file        k_msg.c
 * @brief       kernel message passing routines          
 * @version     V1.2021.06
 * @authors     Yiqing Huang
 * @date        2021 JUN
 *****************************************************************************/

#include "k_inc.h"
#include "k_rtx.h"
#include "k_task.h"
//#include "k_msg.h"

void rt_waitlist_add(DLIST *wait_list, TCB *p_tcb)
{
	TCB *traverse = (TCB *)wait_list->head;
	while (traverse != NULL) {		
		if (p_tcb->timeout < traverse->timeout) {
			insert_before(wait_list, (DNODE *)p_tcb, (DNODE *)traverse);
			break;
		}			
		traverse = traverse->next;
	}
	if (traverse == NULL) {
		push_back(wait_list, (DNODE *)p_tcb);
	}	
}

int k_mbx_create(size_t size) {
#ifdef DEBUG_0
    printf("k_mbx_create: size = %u\r\n", size);
#endif /* DEBUG_0 */

		MAILBOX *mb = &gp_current_task->mb;	
		if (mb->buf_start != NULL) {
			errno = EEXIST;
			return RTX_ERR;
		}		
		if (size < MIN_MSG_SIZE) {
			errno = EINVAL;
			return RTX_ERR;
		}

		mb->space = size;
		mb->buf_start = k_mpool_alloc(MPID_IRAM2, size);
		
		if (mb->buf_start == NULL) {
			return RTX_ERR;
		}
		
		mb->head = mb->buf_start;
		mb->tail = mb->buf_start;
		mb->buf_end = mb->buf_start + size;

		//Clears garbage bits.
		mb->wait_list[0].head = NULL;
    mb->wait_list[1].head = NULL;
    mb->wait_list[2].head = NULL;
    mb->wait_list[3].head = NULL;
		mb->rt_wait_list.head = NULL;

    return gp_current_task->tid;
}

int k_send_msg(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */
	
	U8 *data = (U8 *)buf;
	if (buf == NULL) {
		errno = EFAULT;
		return RTX_ERR;
	}
	
	int length = *(int *)(data);
	if (receiver_tid > MAX_TASKS || length < MIN_MSG_SIZE) {
		errno = EINVAL;
		return RTX_ERR;
	}
	
  TCB *rec_tcb = &g_tcbs[receiver_tid];
	if (rec_tcb->mb.buf_start == NULL) {
		errno = ENOENT;
		return RTX_ERR;
	}
	if (total_size(&rec_tcb->mb) < length) {
		errno = EMSGSIZE;
		return RTX_ERR;
	}	
	
	TCB* p_tcb = gp_current_task;
  while (length > rec_tcb->mb.space) {
    p_tcb->state = BLK_SEND;
    p_tcb->queued_msg = data;
		p_tcb->blocked_on = rec_tcb;
		if (p_tcb->prio == PRIO_RT) {
			pop_front(&rt_queue);
			rt_waitlist_add(&rec_tcb->mb.rt_wait_list, p_tcb);
		}
		else {
			pop_front(&prio_queue[p_tcb->prio - PRIO_OFFSET]);
			push_back(&rec_tcb->mb.wait_list[p_tcb->prio - PRIO_OFFSET], (DNODE *)p_tcb);
		}
    k_tsk_run_new(INVOLUNTARY);
		
		// Check if mailbox still exists
		if (g_tcbs[receiver_tid].mb.buf_start == NULL) {
			errno = ENOENT;
			return RTX_ERR;
		}
		
		// MSG was received
		if (p_tcb->queued_msg == NULL) {
			length = 0;
			break;
		}
  }

  for (int i = 0; i < length; ++i) {
    enqueue(&rec_tcb->mb, data[i]);
  }

  if (rec_tcb->state == BLK_RECV) {
    rec_tcb->state = READY;
    if (rec_tcb->prio == PRIO_RT) {
			rt_queue_add(rec_tcb);
		}
		else {
			push_back(&prio_queue[rec_tcb->prio - PRIO_OFFSET], (DNODE *)rec_tcb);
		}
		
    k_tsk_run_new(INVOLUNTARY);
  }

    return 0;
}

int k_send_msg_nb(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg_nb: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */

	U8 *data = (U8 *)buf;
	if (buf == NULL) {
		errno = EFAULT;
		return RTX_ERR;
	}
	
	int length = *(int *)(data);
	if (receiver_tid > MAX_TASKS || length < MIN_MSG_SIZE) {
		errno = EINVAL;
		return RTX_ERR;
	}
		
  TCB *rec_tcb = &g_tcbs[receiver_tid];
	if (rec_tcb->mb.buf_start == NULL) {
		errno = ENOENT;
		return RTX_ERR;
	}
	if (total_size(&rec_tcb->mb) < length) {
		errno = ENOSPC;
		return RTX_ERR;
	}
	if (length > rec_tcb->mb.space) {
    errno = EMSGSIZE;
    return RTX_ERR;
  }

  for (int i = 0; i < length; ++i) {
    enqueue(&rec_tcb->mb, data[i]);
  }

  if (rec_tcb->state == BLK_RECV) {
    rec_tcb->state = READY;
		if (rec_tcb->prio == PRIO_RT) {
			rt_queue_add(rec_tcb);
		}
		else {
			push_back(&prio_queue[rec_tcb->prio - PRIO_OFFSET], (DNODE *)rec_tcb);
		}
		
		if ( data[5] != KEY_IN ) {
			 k_tsk_run_new(INVOLUNTARY);
		}
  }

    return 0;
}

int k_recv_msg(void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg: buf=0x%x, len=%d\r\n", buf, len);
#endif /* DEBUG_0 */
	
	U8 *data = (U8 *)buf;
	if (buf == NULL) {
		errno = EFAULT;
		return RTX_ERR;
	}
	
	TCB *p_tcb = gp_current_task;
	if (p_tcb->mb.buf_start == NULL) {
		errno = ENOENT;
		return RTX_ERR;
	}

	while (mb_empty(&p_tcb->mb)) {
		p_tcb->state = BLK_RECV;
		pop_front(&prio_queue[p_tcb->prio - PRIO_OFFSET]);
		k_tsk_run_new(INVOLUNTARY);
	}
	
	int msg_length = msg_len(&p_tcb->mb);
	if (msg_length > len) {
		errno = ENOSPC;
		return RTX_ERR;
	}
	
	for (int i = 0; i < msg_length; ++i) {
		data[i] = dequeue(&p_tcb->mb);
	}

	//Now that there's more room in mb, traverse all waiting lists to receive more messages.
	TCB *traverse = (TCB *)p_tcb->mb.rt_wait_list.head;
	while (traverse != NULL) {

		if (p_tcb->mb.space >= *(int *)(traverse->queued_msg)) {
			for (int i = 0; i < *(int *)(traverse->queued_msg); ++i) {
				enqueue(&p_tcb->mb, traverse->queued_msg[i]);
			}
			traverse->queued_msg = NULL;
			traverse->state = READY;
			remove(&p_tcb->mb.rt_wait_list, (DNODE *)traverse);
			rt_queue_add(traverse);
		}
		traverse = traverse->next;
	}
	for (int prio = 0; prio < 4; ++prio) {
			traverse = (TCB *)p_tcb->mb.wait_list[prio].head;
			while (traverse != NULL) {

				if (p_tcb->mb.space >= *(int *)(traverse->queued_msg)) {
					for (int i = 0; i < *(int *)(traverse->queued_msg); ++i) {
						enqueue(&p_tcb->mb, traverse->queued_msg[i]);
					}
					traverse->queued_msg = NULL;
					traverse->state = READY;
					remove(&p_tcb->mb.wait_list[prio], (DNODE *)traverse);
					push_back(&prio_queue[traverse->prio - PRIO_OFFSET], (DNODE *)traverse);
				}
				traverse = traverse->next;
			}
	}

  k_tsk_run_new(INVOLUNTARY);

    return 0;
}

int k_recv_msg_nb(void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg_nb: buf=0x%x, len=%d\r\n", buf, len);
#endif /* DEBUG_0 */

	U8 *data = (U8 *)buf;
	if (buf == NULL) {
		errno = EFAULT;
		return RTX_ERR;
	}
	
	TCB *p_tcb = gp_current_task;
	if (p_tcb->mb.buf_start == NULL) {
		errno = ENOENT;
		return RTX_ERR;
	}
	
	if (mb_empty(&p_tcb->mb)) {
		errno = ENOMSG;
		return RTX_ERR;
	}
	
	int msg_length = msg_len(&p_tcb->mb);
	if (msg_length > len) {
		errno = ENOSPC;
		return RTX_ERR;
	}

	for (int i = 0; i < msg_length; ++i) {
		data[i] = dequeue(&p_tcb->mb);
	}

	//Now that there's more room in mb, traverse all waiting lists to receive more messages.
	TCB *traverse = (TCB *)p_tcb->mb.rt_wait_list.head;
	while (traverse != NULL) {

		if (p_tcb->mb.space >= *(int *)(traverse->queued_msg)) {
			for (int i = 0; i < *(int *)(traverse->queued_msg); ++i) {
				enqueue(&p_tcb->mb, traverse->queued_msg[i]);
			}
			traverse->queued_msg = NULL;
			traverse->state = READY;
			remove(&p_tcb->mb.rt_wait_list, (DNODE *)traverse);
			rt_queue_add(traverse);
		}
		traverse = traverse->next;
	}
	for (int prio = 0; prio < 4; ++prio) {
			traverse = (TCB *)p_tcb->mb.wait_list[prio].head;
			while (traverse != NULL) {

				if (p_tcb->mb.space >= *(int *)(traverse->queued_msg)) {
					for (int i = 0; i < *(int *)(traverse->queued_msg); ++i) {
						enqueue(&p_tcb->mb, traverse->queued_msg[i]);
					}
					traverse->queued_msg = NULL;
					traverse->state = READY;
					remove(&p_tcb->mb.wait_list[prio], (DNODE *)traverse);
					push_back(&prio_queue[traverse->prio - PRIO_OFFSET], (DNODE *)traverse);
				}
				traverse = traverse->next;
			}
	}

  k_tsk_run_new(INVOLUNTARY);

    return 0;
}

int k_mbx_ls(task_t *buf, size_t count) {
#ifdef DEBUG_0
    printf("k_mbx_ls: buf=0x%x, count=%u\r\n", buf, count);
#endif /* DEBUG_0 */
	
	  if (buf == NULL || count == 0) {
       errno = EFAULT;
       return RTX_ERR;
    }

    int tasks = 0;
    for (int i = 0; i < MAX_TASKS && tasks <= count; ++i) {
       if (g_tcbs[i].mb.buf_start != NULL) {
           buf[tasks] = g_tcbs[i].tid;
           ++tasks;
       }
    }

    return tasks;
}

int k_mbx_get(task_t tid)
{
#ifdef DEBUG_0
    printf("k_mbx_get: tid=%u\r\n", tid);
#endif /* DEBUG_0 */
		if (g_tcbs[tid].mb.buf_start == NULL) {
			errno = ENOENT;
			return RTX_ERR;
		}
	
    return g_tcbs[tid].mb.space;
}
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

