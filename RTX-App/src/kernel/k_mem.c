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
 * @file        k_mem.c
 * @brief       Kernel Memory Management API C Code
 *
 * @version     V1.2021.01.lab2
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @note        skeleton code
 *
 *****************************************************************************/
#include "k_inc.h"
#include "k_mem.h"
#include "btree.h"

DLIST free_list_1 [MEM1_HEIGHT + 1];        
U8 bit_tree_1[32];
        
DLIST free_list_2 [MEM2_HEIGHT + 1];       
U8 bit_tree_2[256];

#ifdef DEBUG_2 
	U32 mem2_space = 0x8000;
	U32 mem1_space = 0x1000;
#endif

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
                              |                           |     |
                              |---------------------------|     |
                              |                           |     |
                              |      other data           |     |
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

/* note list[n] is for blocks with order of n */
mpool_t k_mpool_create (int algo, U32 start, U32 end)
{
#ifdef DEBUG_0
    printf("k_mpool_init: algo = %d\r\n\r", algo);
    printf("k_mpool_init: RAM range: [0x%x, 0x%x].\r\n\r", start, end);
#endif /* DEBUG_0 */    
    
    if (algo != BUDDY ) {
        errno = EINVAL;
        return RTX_ERR;
    }
    
    if ( start == RAM1_START) {
        free_list_1[0].head = (void *) RAM1_START;
				free_list_1[0].head->prev = NULL;
				free_list_1[0].head->next = NULL;
        for (U8 level = 1 ; level <= MEM1_HEIGHT ; level++){
            free_list_1[level].head = NULL;
        }

    } else if ( start == RAM2_START) {
        free_list_2[0].head = (void *) RAM2_START;
				free_list_2[0].head->prev = NULL;
				free_list_2[0].head->next = NULL;
        for (U8 level = 1 ; level <= MEM2_HEIGHT ; level++){
            free_list_2[level].head = NULL;
        }
    } else {
        errno = EINVAL;
        return RTX_ERR;
    }
    
    return start == RAM1_START ? MPID_IRAM1 : MPID_IRAM2;
}

void *k_mpool_alloc (mpool_t mpid, size_t size)
{
	if (size <= 0) {
		return NULL;
	}
	if (size < 32) {
		size = 32;
	}
	if ((mpid != MPID_IRAM1) && (mpid != MPID_IRAM2)) {
		errno = EINVAL;
		return NULL;
	}
	
	DNODE *block_ptr = NULL;
	DNODE *block_mid_ptr = NULL;
	
#ifdef DEBUG_0
    printf("k_mpool_alloc: mpid = %d, size = %d, 0x%x\r\n\r", mpid, size, size);
#endif /* DEBUG_0 */
	
	if (mpid == MPID_IRAM1) {
		
		unsigned int target_level = MEM1_POWER - log2_ceil(size);
		
		if ((bottom_up(mpid, target_level) == -1) || (size > RAM1_SIZE)) {
			errno = ENOMEM;
			return NULL;
		}
		
		#ifdef DEBUG_2
			mem1_space = mem1_space - upow(2, MEM1_POWER - target_level); 
		#endif
	
		if (!empty(&free_list_1[target_level])) {
			block_ptr = free_list_1[target_level].head;
			pop_front(&free_list_1[target_level]);
			set_bit(bit_tree_1, get_index(target_level, get_position(mpid, block_ptr, target_level)));
		}
		else {
			for (int current_level = target_level; current_level >= 0; --current_level) {
				
				if (!empty(&free_list_1[current_level])) {
						
					block_ptr = free_list_1[current_level].head;
					for (; current_level < target_level; ++current_level) {
						set_bit(bit_tree_1, get_index(current_level, get_position(mpid, block_ptr, current_level)));
							
						block_mid_ptr = split_addr(mpid, current_level, block_ptr);
						pop_front(&free_list_1[current_level]);
							
						push_front(&free_list_1[current_level + 1], block_mid_ptr);
						push_front(&free_list_1[current_level + 1], block_ptr);
					}

					set_bit(bit_tree_1, get_index(current_level, get_position(mpid, block_ptr, current_level)));
					pop_front(&free_list_1[current_level]);
					break;
				}
			}
		}
	}
	
	if (mpid == MPID_IRAM2) {
		
    unsigned int target_level = MEM2_POWER - log2_ceil(size);
		
		if ((bottom_up(mpid, target_level) == -1) || (size > RAM2_SIZE)) {
			errno = ENOMEM;
			return NULL;
		}
		
		#ifdef DEBUG_2
			mem2_space = mem2_space - upow(2, MEM2_POWER - target_level);
		#endif
		
		if (!empty(&free_list_2[target_level])) {
			block_ptr = free_list_2[target_level].head;
			pop_front(&free_list_2[target_level]);
			set_bit(bit_tree_2, get_index(target_level, get_position(mpid, block_ptr, target_level)));
		}
		else {
			for (int current_level = target_level; current_level >= 0; --current_level) {
				
				if (!empty(&free_list_2[current_level])) {
						
					block_ptr = free_list_2[current_level].head;
					for (; current_level < target_level; ++current_level) {
						set_bit(bit_tree_2, get_index(current_level, get_position(mpid, block_ptr, current_level)));
							
						block_mid_ptr = split_addr(mpid, current_level, block_ptr);
						pop_front(&free_list_2[current_level]);
							
						push_front(&free_list_2[current_level + 1], block_mid_ptr);
						push_front(&free_list_2[current_level + 1], block_ptr);
					}

					set_bit(bit_tree_2, get_index(current_level, get_position(mpid, block_ptr, current_level)));
					pop_front(&free_list_2[current_level]);
					break;
				}
			}
		}
	}

  return block_ptr;
}

/*
 * Function: k_mpool_dealloc
 * ----------------------------
 *
 *   mpid: Memory Pool ID
 *   ptr: Address of block to be deallocated
 *
 *   returns: 0 if memory is deallocated, or -1 if error.
 *
 *	 1. Start at relative position of pointer at bottom of tree.
 *	 2. If index is allocated, clear bit in tree and check if buddy is also free.
 *			a) If buddy free, remove buddy from free list and then repeat step 2 at above level for parent node (coalesce).
 *			b) If buddy is not free, just push the current node to the free list.
 *	
 */
int k_mpool_dealloc(mpool_t mpid, void *ptr)
{
	//Input validation
	if (ptr == NULL) {
		return RTX_OK;
	}
	if (mpid != MPID_IRAM1 && mpid != MPID_IRAM2) {
		errno = EINVAL;
		return RTX_ERR;
	}
	
	//Check if pointer falls in appropriate memory pool
	if (mpid == MPID_IRAM1 && !((unsigned int)ptr >= RAM1_START && (unsigned int)ptr < RAM1_END)) {
		errno = EFAULT;
		return RTX_ERR;
	}
	if (mpid == MPID_IRAM2 && !((unsigned int)ptr >= RAM2_START && (unsigned int)ptr < RAM2_END)) {
		errno = EFAULT;
		return RTX_ERR;
	}
	
#ifdef DEBUG_0
    printf("k_mpool_dealloc: mpid = %d, ptr = 0x%x\r\n\r", mpid, ptr);
#endif /* DEBUG_0 */
	
	if (mpid == MPID_IRAM1) {
		
		#ifdef DEBUG_2
			U8 flag = 0;
		#endif
		
		unsigned int position = get_position(mpid, ptr, MEM1_HEIGHT);
		for (int level = MEM1_HEIGHT; level >= 0; --level) {
			unsigned int index = get_index(level, position);
			
			if (is_allocated(bit_tree_1, index)) {
				clear_bit(bit_tree_1, index);
				
				#ifdef DEBUG_2
					if (!flag) {
						mem1_space = mem1_space + upow(2, MEM1_POWER - level);
						flag = 1;
					}
				#endif
				
				unsigned int buddy_index = get_buddy(level, position);
				if (!is_allocated(bit_tree_1, buddy_index) && (index != 0)) {
					remove(&free_list_1[level], get_address(mpid, level, position + (buddy_index - index)));
					position /= 2;
					continue;
				}
				
				DNODE *block_addr = get_address(mpid, level, position);
				block_addr->next = NULL;
				block_addr->prev = NULL;
				push_front(&free_list_1[level], block_addr);
				break;
			}
			
			position /= 2;
		}
		
	}
	if (mpid == MPID_IRAM2) {
		
		#ifdef DEBUG_2
			U8 flag = 0;
		#endif
		
		unsigned int position = get_position(mpid, ptr, MEM2_HEIGHT);
		for (int level = MEM2_HEIGHT; level >= 0; --level) {
			unsigned int index = get_index(level, position);
			
			if (is_allocated(bit_tree_2, index)) {
				clear_bit(bit_tree_2, index);
				
				#ifdef DEBUG_2
					if (!flag) {
						mem2_space = mem2_space + upow(2, MEM2_POWER - level);
						flag = 1;
					}
				#endif
				
				unsigned int buddy_index = get_buddy(level, position);
				if (!is_allocated(bit_tree_2, get_buddy(level, position)) && (index != 0)) {
					remove(&free_list_2[level], get_address(mpid, level, position + (buddy_index - index)));
					position /= 2;
					continue;
				}
				
				DNODE *block_addr = get_address(mpid, level, position);
				block_addr->next = NULL;
				block_addr->prev = NULL;
				push_front(&free_list_2[level], block_addr);
				break;
			}
			
			position /= 2;
		}
	}
    
    return RTX_OK; 
}

int k_mpool_dump (mpool_t mpid)
{
#ifdef DEBUG_0
    printf("k_mpool_dump: mpid = %d\r\n", mpid);
#endif /* DEBUG_0 */

    int block_count = 0;

    if (mpid == MPID_IRAM1) {

        for (int i = MEM1_HEIGHT; i >= 0; i--) {

            if (free_list_1[i].head) { 
                DNODE *traverse = free_list_1[i].head;

                while (traverse) {
                    block_count++;
                    printf("0x%x: 0x%x\n\r", traverse, upow(2, MIN_POWER - i + MEM1_HEIGHT));
                    traverse = traverse->next;
                }
            }
        }
    }
		else if (mpid == MPID_IRAM2)  {

        for (int i = MEM2_HEIGHT; i >= 0; i--) {

            if (free_list_2[i].head) { 
                DNODE *traverse = free_list_2[i].head;

                while (traverse) {
                    block_count++;
                    printf("0x%x: 0x%x\n\r", traverse, upow(2, MIN_POWER - i + MEM2_HEIGHT));
                    traverse = traverse->next;
                }
            }
        }
    }
    printf("%d free memory block(s) found\n\r", block_count);
    return block_count;
}

int bottom_up(mpool_t mpid, U8 level)
{	
		for (int i = level; i >= 0; --i) {
			if (mpid == MPID_IRAM1 && free_list_1[i].head) {
					return level;
			}
			if (mpid == MPID_IRAM2 && free_list_2[i].head) {
					return level;
			}
		}

    return -1;  
}
 
int k_mem_init(int algo)
{
#ifdef DEBUG_0
    printf("k_mem_init: algo = %d\r\n\r", algo);
#endif /* DEBUG_0 */
        
    if ( k_mpool_create(algo, RAM1_START, RAM1_END) < 0 ) {
        return RTX_ERR;
    }
    
    if ( k_mpool_create(algo, RAM2_START, RAM2_END) < 0 ) {
        return RTX_ERR;
    }
    
    return RTX_OK;
}

/**
 * @brief allocate kernel stack statically
 */
U32* k_alloc_k_stack(task_t tid)
{   
    U32 *sp = k_mpool_alloc(MPID_IRAM2, KERN_STACK_SIZE);
	  if (sp == NULL) {
      return NULL;
    }
    sp = (U32*) ((U32)sp + KERN_STACK_SIZE);
    
    return sp;
}

/**
 * @brief allocate user/process stack dynamically
 */

U32* k_alloc_p_stack(task_t tid, U32 task_size)
{
    U32 *sp = k_mpool_alloc(MPID_IRAM2, task_size);
    if (sp == NULL) {
      return NULL;
    }
    sp = (U32*) ((U32)sp + task_size);
    
    return sp;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

