/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2022 Yiqing Huang
 *                          All rights reserved.
 *
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
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_task.c
 * @brief       task management C file
 * @version     V1.2021.05
 * @authors     Yiqing Huang
 * @date        2021 MAY
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only three simple tasks and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/


#include "k_inc.h"
//#include "k_task.h"
#include "k_rtx.h"

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;    // the current RUNNING task
TCB             g_tcbs[MAX_TASKS];          // an array of TCBs
//TASK_INIT       g_null_task_info;           // The null task info
U32             g_num_active_tasks = 0;     // number of non-dormant tasks

// index goes from highest to lowest priority
DLIST prio_queue[4];

// sorted from lowest to highest deadline
DLIST rt_queue;

// sorted by relative timeouts
DLIST timeout_list;

/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:
                   RAM1_END-->+---------------------------+ High Address
                              |                           |
                              |                           |
                              |       MPID_IRAM1          |
                              |   (for user space heap  ) |
                              |                           |
                 RAM1_START-->|---------------------------|
                              |                           |
                              |  unmanaged free space     |
                              |                           |
&Image$$RW_IRAM1$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      PROC_STACK_SIZE      |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      KERN_STACK_SIZE      |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      KERN_STACK_SIZE      |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      KERN_STACK_SIZE      |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      KERN_STACK_SIZE      |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |        Code + RO          |     |
                              |                           |     V
                 IRAM1_BASE-->+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */
 
void rt_queue_add(TCB *p_tcb)
{
	TCB *traverse = (TCB *)rt_queue.head;
	while (traverse != NULL) {		
		if (p_tcb->timeout < traverse->timeout) {
			insert_before(&rt_queue, (DNODE *)p_tcb, (DNODE *)traverse);
			break;
		}			
		traverse = traverse->next;
	}
	if (traverse == NULL) {
		push_back(&rt_queue, (DNODE *)p_tcb);
	}	
}

void timeout_list_add(TCB *p_tcb)
{
	TCB *traverse = (TCB *)timeout_list.head;
	p_tcb->timeout -= g_timer_count;
	while (traverse != NULL) {
		if (p_tcb->timeout < traverse->timeout) {
			traverse->timeout -= p_tcb->timeout;
			insert_before(&timeout_list, (DNODE *)p_tcb, (DNODE *)traverse);
			break;
		}
		p_tcb->timeout -= traverse->timeout;
		traverse = traverse->next;
	}
	if (traverse == NULL) {
		push_back(&timeout_list, (DNODE *)p_tcb);
	}
}


/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 * @note    you need to change this one to be a priority scheduler
 *
 *****************************************************************************/

TCB *scheduler(void)
{
	  if (rt_queue.head != NULL) {
        return (TCB *) rt_queue.head;
    }
    if (prio_queue[0].head != NULL) {
        return (TCB *) prio_queue[0].head;
    }
    if (prio_queue[1].head != NULL) {
        return (TCB *) prio_queue[1].head;
    }
    if (prio_queue[2].head != NULL) {
        return (TCB *) prio_queue[2].head;
    }
    if (prio_queue[3].head != NULL) {
        return (TCB *) prio_queue[3].head;
    }

    return &g_tcbs[TID_NULL];
}

/**
 * @brief initialize daemon tasks in system
 */
void k_tsk_init_null(TASK_INIT *p_task)
{
    p_task->prio         = PRIO_NULL;
    p_task->priv         = 0;
    p_task->tid          = TID_NULL;
    p_task->ptask        = &task_null;
    p_task->u_stack_size = PROC_STACK_SIZE;
}
void k_tsk_init_kcd(TASK_INIT *p_task)
{
    p_task->prio         = HIGH;
    p_task->priv         = 0;
    p_task->tid          = TID_KCD;
    p_task->ptask        = &task_kcd;
    p_task->u_stack_size = PROC_STACK_SIZE;
}
void k_tsk_init_cdisp(TASK_INIT *p_task)
{
    p_task->prio         = HIGH;
    p_task->priv         = 1;
    p_task->tid          = TID_CON;
    p_task->ptask        = &task_cdisp;
    p_task->u_stack_size = PROC_STACK_SIZE;
}
void k_tsk_init_wclck(TASK_INIT *p_task)
{
    p_task->prio         = HIGH;
    p_task->priv         = 0;
    p_task->tid          = TID_WCLCK;
    p_task->ptask        = &task_wall_clock;
    p_task->u_stack_size = PROC_STACK_SIZE;
}

/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 * @see         k_tsk_create_first
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(TASK_INIT *task, int num_tasks)
{
    if (num_tasks > MAX_TASKS - 1) {
        return RTX_ERR;
    }

    prio_queue[0].head = NULL;
    prio_queue[1].head = NULL;
    prio_queue[2].head = NULL;
    prio_queue[3].head = NULL;
		rt_queue.head = NULL;
		timeout_list.head = NULL;
    
    TASK_INIT taskinfo[4];
    
    // create and start NULL task
    k_tsk_init_null(&taskinfo[0]);
    if ( k_tsk_create_new(&taskinfo[0], &g_tcbs[TID_NULL], TID_NULL) == RTX_OK ) {
        g_num_active_tasks = 1;
        gp_current_task = &g_tcbs[TID_NULL];
    } else {
        g_num_active_tasks = 0;
        return RTX_ERR;
    }

    //create and start kcd and cdisp tasks
		k_tsk_init_cdisp(&taskinfo[1]);
    if ( k_tsk_create_new(&taskinfo[1], &g_tcbs[TID_CON], TID_CON) == RTX_OK ) {
				TCB *p_tcb = &g_tcbs[TID_CON];
				push_back(&prio_queue[g_tcbs[TID_CON].prio - PRIO_OFFSET], (DNODE *)p_tcb);
        g_num_active_tasks++;
    }
    k_tsk_init_kcd(&taskinfo[2]);
    if ( k_tsk_create_new(&taskinfo[2], &g_tcbs[TID_KCD], TID_KCD) == RTX_OK ) {
				TCB *p_tcb = &g_tcbs[TID_KCD];
				push_back(&prio_queue[g_tcbs[TID_KCD].prio - PRIO_OFFSET], (DNODE *)p_tcb);
        g_num_active_tasks++;
    }
		
		//create and start WCLCK task
		k_tsk_init_wclck(&taskinfo[3]);
    if ( k_tsk_create_new(&taskinfo[3], &g_tcbs[TID_WCLCK], TID_WCLCK) == RTX_OK ) {
				TCB *p_tcb = &g_tcbs[TID_WCLCK];
				push_back(&prio_queue[g_tcbs[TID_WCLCK].prio - PRIO_OFFSET], (DNODE *)p_tcb);
        g_num_active_tasks++;
    }
    
    // create the rest of the tasks and push to ready queue
    for ( int i = 0; i < num_tasks; i++ ) {
        TCB *p_tcb = &g_tcbs[i+1];
        if (k_tsk_create_new(&task[i], p_tcb, i+1) == RTX_OK) {
            push_back(&prio_queue[p_tcb->prio - PRIO_OFFSET], (DNODE *)p_tcb);
            g_num_active_tasks++;
        }
    }
		
		for ( int i = 0; i < MAX_TASKS; i++ ) {
			g_tcbs[i].mb.buf_start = NULL;
		}
    
    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task initialization structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR3)
 *              then we stack up the kernel initial context (kLR, kR4-kR12, PSP, CONTROL)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              20 registers in total
 * @note        YOU NEED TO MODIFY THIS FILE!!!
 *****************************************************************************/
int k_tsk_create_new(TASK_INIT *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RTE;

    U32 *usp;
    U32 *ksp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

    p_tcb->k_stack_size = KERN_STACK_SIZE;
    if (p_taskinfo->u_stack_size < PROC_STACK_SIZE) {
        p_tcb->u_stack_size = PROC_STACK_SIZE;
    }
    else {
        p_tcb->u_stack_size = p_taskinfo->u_stack_size;
    }

    p_tcb->tid   = tid;
    p_tcb->prio  = p_taskinfo->prio;
    p_tcb->priv  = p_taskinfo->priv;
    p_tcb->ptask = p_taskinfo->ptask;
    
    /*---------------------------------------------------------------
     *  Step1: allocate user stack for the task
     *         stacks grows down, stack base is at the high address
     * ATTENTION: you need to modify the following three lines of code
     *            so that you use your own dynamic memory allocator
     *            to allocate variable size user stack.
     * -------------------------------------------------------------*/
    
    // if NULL task, usp is already allocated
    usp = (tid == TID_NULL) ? (U32 *)p_tcb->u_sp_base : k_alloc_p_stack(tid, p_tcb->u_stack_size);
    if (usp == NULL) {
        return RTX_ERR;
    }

    p_tcb->u_sp_base = (U32)usp;

    /*-------------------------------------------------------------------
     *  Step2: create task's thread mode initial context on the user stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the exception stack frame saved on the user stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         8 registers listed in push order
     *         <xPSR, PC, uLR, uR12, uR3, uR2, uR1, uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    
    *(--usp) = INITIAL_xPSR;             // xPSR: Initial Processor State
    *(--usp) = (U32) (p_taskinfo->ptask);// PC: task entry point
        
    // uR14(LR), uR12, uR3, uR3, uR1, uR0, 6 registers
    for ( int j = 0; j < 6; j++ ) {
        
#ifdef DEBUG_0
        *(--usp) = 0xDEADAAA0 + j;
#else
        *(--usp) = 0x0;
#endif
    }

    p_tcb->u_sp = (U32)usp;
    
    // allocate kernel stack for the task
    ksp = k_alloc_k_stack(tid);
    if ( ksp == NULL ) {
        return RTX_ERR;
    }
    
    p_tcb->k_sp_base = (U32)ksp;

    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         12 registers listed in push order
     *         <kLR, kR4-kR12, PSP, CONTROL>
     * -------------------------------------------------------------*/
    // a task never run before directly exit
    *(--ksp) = (U32) (&SVC_RTE);
    // kernel stack R4 - R12, 9 registers
#define NUM_REGS 9    // number of registers to push
      for ( int j = 0; j < NUM_REGS; j++) {        
#ifdef DEBUG_0
        *(--ksp) = 0xDEADCCC0 + j;
#else
        *(--ksp) = 0x0;
#endif
    }
        
    // put user sp on to the kernel stack
    *(--ksp) = (U32) usp;
    
    // save control register so that we return with correct access level
    if (p_taskinfo->priv == 1) {  // privileged 
        *(--ksp) = __get_CONTROL() & ~BIT(0); 
    } else {                      // unprivileged
        *(--ksp) = __get_CONTROL() | BIT(0);
    }

    p_tcb->msp = ksp;
    p_tcb->state = READY;

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param       p_tcb_old, the old tcb that was in RUNNING
 * @return      RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre         gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note        caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * @note        The control register setting will be done by the caller
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PRESERVE8
        EXPORT  K_RESTORE
        
        PUSH    {R4-R12, LR}                // save general pupose registers and return address
        MRS     R4, CONTROL                 
        MRS     R5, PSP
        PUSH    {R4-R5}                     // save CONTROL, PSP
        STR     SP, [R0, #TCB_MSP_OFFSET]   // save SP to p_old_tcb->msp
K_RESTORE
        LDR     R1, =__cpp(&gp_current_task)
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_MSP_OFFSET]   // restore msp of the gp_current_task
        POP     {R4-R5}
        MSR     PSP, R5                     // restore PSP
        MSR     CONTROL, R4                 // restore CONTROL
        ISB                                 // flush pipeline, not needed for CM3 (architectural recommendation)
        POP     {R4-R12, PC}                // restore general purpose registers and return address
}


__asm void k_tsk_start(void)
{
        PRESERVE8
        B K_RESTORE
}

/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(BOOL voluntary)
{
    TCB *p_tcb_old = NULL;
    
    if (gp_current_task == NULL) {
        return RTX_ERR;
    }

	  // push old task to end of its queue if voluntary
    p_tcb_old = gp_current_task;
    if (voluntary && p_tcb_old->tid != TID_NULL) {
				pop_front(&prio_queue[p_tcb_old->prio - PRIO_OFFSET]);
        push_back(&prio_queue[p_tcb_old->prio - PRIO_OFFSET], (DNODE *)p_tcb_old);
    }

    gp_current_task = scheduler();

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
				if (p_tcb_old->state == RUNNING) {
					  p_tcb_old->state = READY;  			// change state of the to-be-switched-out tcb (only if not blocked)
				};
				
				#ifdef DEBUG_2
					p_tcb_old->u_sp = (U32)__get_PSP();
					if (p_tcb_old->u_sp_base - p_tcb_old->u_sp > p_tcb_old->u_stack_size) {
						printf("Task %u stack overflow.\n\r", p_tcb_old->tid);
					}
				#endif

        k_tsk_switch(p_tcb_old);            // switch kernel stacks       
    }

    return RTX_OK;
}

 
/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
		if (gp_current_task->prio == PRIO_RT) {
			return RTX_ERR;
		}
    return k_tsk_run_new(VOLUNTARY);
}

/**
 * @brief   get task identification
 * @return  the task ID (TID) of the calling task
 */
task_t k_tsk_gettid(void)
{
    return gp_current_task->tid;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U32 stack_size)
{
#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = 0x%x, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */

    if (prio < HIGH || prio > LOWEST) {
      errno = EINVAL;
      return RTX_ERR;
    }

    for (*task = 1; *task < 10; ++*task) {
        if (g_tcbs[*task].state == DORMANT) { //Check if state init to 0 on board
            break;
        }
    }
    if (*task == MAX_TASKS) {
        errno = EAGAIN;
        return RTX_ERR;
    }

    TASK_INIT p_task_info;
    p_task_info.tid = *task;
    p_task_info.prio = prio;
    p_task_info.u_stack_size = stack_size;
    p_task_info.ptask = task_entry;
    p_task_info.priv = 0;

    TCB *p_tcb = &g_tcbs[*task];
    if (k_tsk_create_new(&p_task_info, p_tcb, *task) != RTX_OK) {
        return RTX_ERR;
    };
    push_back(&prio_queue[p_tcb->prio - PRIO_OFFSET], (DNODE *)p_tcb);

    g_num_active_tasks++;
    k_tsk_run_new(INVOLUNTARY);

    return RTX_OK;

}

void k_tsk_exit(void) 
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */
    TCB *p_tcb_old = gp_current_task;
		if (p_tcb_old->prio == PRIO_RT) {
			pop_front(&rt_queue);
		}
		else {
			pop_front(&prio_queue[p_tcb_old->prio - PRIO_OFFSET]);
		}
	
		//Delete mailbox and unblock all waiting tasks
		if (p_tcb_old->mb.buf_start != NULL) {
			
				for (int prio = 0; prio < 4; ++prio) {
					
						TCB *traverse = (TCB *)p_tcb_old->mb.wait_list[prio].head;
						while (traverse != NULL) {
							
							traverse->state = READY;
							remove(&p_tcb_old->mb.wait_list[prio], (DNODE *)traverse);
							if (traverse->prio == PRIO_RT) {
								rt_queue_add(traverse);
							}
							else {
								push_back(&prio_queue[traverse->prio - PRIO_OFFSET], (DNODE *)traverse);
							}
							traverse = traverse->next;
						}
				}
				k_mpool_dealloc(MPID_IRAM2, p_tcb_old->mb.buf_start);
				p_tcb_old->mb.buf_start = NULL;
		}
		
		//Dealloc user and kernel stacks
		void *stack_address = (void *) (p_tcb_old->u_sp_base - p_tcb_old->u_stack_size);
    k_mpool_dealloc(MPID_IRAM2, stack_address);
		stack_address = (void *) (p_tcb_old->k_sp_base - p_tcb_old->k_stack_size);
		k_mpool_dealloc(MPID_IRAM2, stack_address);
		
    p_tcb_old->state = DORMANT;
    g_num_active_tasks--;

    gp_current_task = scheduler();
    gp_current_task->state = RUNNING;
    k_tsk_switch(p_tcb_old); 

    return;
}

int k_tsk_set_prio(task_t task_id, U8 prio) 
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */
    TCB *p_tcb = &g_tcbs[task_id];
    if ((p_tcb->state == DORMANT) || (p_tcb->prio == prio)) {
      return RTX_OK;
    }
    if ((p_tcb->priv == 1 && gp_current_task->priv == 0) || (prio == PRIO_RT && p_tcb->prio != PRIO_RT) || (p_tcb->prio == PRIO_RT && prio != PRIO_RT)) {
      errno = EPERM;
      return RTX_ERR;
    }
    if ((prio < HIGH) || (prio > LOWEST) || (p_tcb->tid == TID_NULL)) {
      errno = EINVAL;
      return RTX_ERR;
    }
		
		if (p_tcb->state == BLK_SEND) {
			remove(&p_tcb->blocked_on->mb.wait_list[p_tcb->prio - PRIO_OFFSET], (DNODE *)p_tcb);
			p_tcb->prio = prio;
			push_back(&p_tcb->blocked_on->mb.wait_list[prio - PRIO_OFFSET], (DNODE *)p_tcb);
		}
		else if (p_tcb->state == BLK_RECV) {
			p_tcb->prio = prio;
		}
		else {
			remove(&prio_queue[p_tcb->prio - PRIO_OFFSET], (DNODE *)p_tcb);
			p_tcb->prio = prio;
			push_back(&prio_queue[prio - PRIO_OFFSET], (DNODE *)p_tcb);

			k_tsk_run_new(INVOLUNTARY);
		}

    return RTX_OK;
}

/**
 * @brief   Retrieve task internal information 
 */
int k_tsk_get(task_t tid, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get: entering...\n\r");
    printf("tid = %d, buffer = 0x%x.\n\r", tid, buffer);
#endif /* DEBUG_0 */    
    if (buffer == NULL) {
        errno = EFAULT;
        return RTX_ERR;
    }
    if (tid >= MAX_TASKS) {
        errno = EINVAL;
        return RTX_ERR;
    }
		
		TCB *task_tcb = &g_tcbs[tid];
    
    buffer->tid           = tid;
    buffer->prio          = task_tcb->prio;
    buffer->u_stack_size  = task_tcb->u_stack_size;
    buffer->priv          = task_tcb->priv;
    buffer->ptask         = task_tcb->ptask;
    buffer->k_sp_base     = task_tcb->k_sp_base;
    buffer->k_stack_size  = task_tcb->k_stack_size;
    buffer->state         = task_tcb->state;
    buffer->u_sp_base     = task_tcb->u_sp_base;

    if (tid == gp_current_task->tid) {
        buffer->k_sp = (U32)__get_MSP();
        buffer->u_sp = (U32)__get_PSP();
    }
    else {
        buffer->k_sp = (U32)task_tcb->msp;
        buffer->u_sp = task_tcb->u_sp;
    }

    return RTX_OK;     
}

int k_tsk_ls(task_t *buf, size_t count)
{
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%u\r\n", buf, count);
#endif /* DEBUG_0 */

    if (buf == NULL || count == 0) {
        errno = EFAULT;
        return RTX_ERR;
    }

    int tasks = 0;
    for (int i = 0; i < MAX_TASKS && tasks <= count; ++i) {
        if (g_tcbs[i].state != DORMANT) {
            buf[tasks] = g_tcbs[i].tid;
            ++tasks;
        }
    }

    return tasks;
}

int k_rt_tsk_set(TIMEVAL *p_tv)
{
#ifdef DEBUG_0
    printf("k_rt_tsk_set: p_tv = 0x%x\r\n", p_tv);
#endif /* DEBUG_0 */
	TCB *p_tcb = gp_current_task;
	if (p_tcb->prio == PRIO_RT) {
		errno = EPERM;
		return RTX_ERR;
	}
	
	U32 usec_period = p_tv->sec * USEC_IN_SEC + p_tv->usec;
	if ( (usec_period % (RTX_TICK_SIZE * MIN_PERIOD) != 0) && (usec_period != 0) ) {
		errno = EINVAL;
		return RTX_ERR;
	}
	
	pop_front(&prio_queue[p_tcb->prio - PRIO_OFFSET]);
	
	// Queue should be empty
	push_back(&rt_queue, (DNODE *) p_tcb);
	
	p_tcb->prio = PRIO_RT;
	p_tcb->state = RUNNING;
	p_tcb->deadline = usec_period / RTX_TICK_SIZE;
	p_tcb->release_time = g_timer_count;
	p_tcb->timeout = p_tcb->release_time + p_tcb->deadline;

    return RTX_OK;   
}

int k_rt_tsk_susp(void)
{
#ifdef DEBUG_0
    printf("k_rt_tsk_susp: entering\r\n");
#endif /* DEBUG_0 */
	TCB *p_tcb = gp_current_task;
	if (p_tcb->prio != PRIO_RT) {
		errno = EPERM;
		return RTX_ERR;
	}
	
	// if deadline not missed, suspend.
	// else, immediately add task to rt_queue
	if (p_tcb->timeout >= g_timer_count) {
		
		pop_front(&rt_queue);
		p_tcb->state = SUSPENDED;	
		
		timeout_list_add(p_tcb);
		
		k_tsk_run_new(INVOLUNTARY);
		p_tcb->state = RUNNING; // Wake up
	}
	else {
		pop_front(&rt_queue);

		p_tcb->state = READY;
		p_tcb->release_time = g_timer_count;
		p_tcb->timeout = p_tcb->release_time + p_tcb->deadline;
		
		rt_queue_add(p_tcb);
		
		k_tsk_run_new(INVOLUNTARY);
	}

    return RTX_OK;
}

int k_rt_tsk_get(task_t tid, TIMEVAL *buffer)
{
#ifdef DEBUG_0
    printf("k_rt_tsk_get: entering...\n\r");
    printf("tid = %d, buffer = 0x%x.\n\r", tid, buffer);
#endif /* DEBUG_0 */    
		TCB *p_tcb = &g_tcbs[tid];
		if (p_tcb->prio != PRIO_RT) {
			errno = EINVAL;
			return RTX_ERR;
		}
		if (gp_current_task->prio != PRIO_RT) {
			errno = EPERM;
			return RTX_ERR;
		}		
    
    buffer->sec  = p_tcb->deadline / USEC_IN_SEC;
    buffer->usec = p_tcb->deadline - buffer->sec * USEC_IN_SEC;
    
    return RTX_OK;
}
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

