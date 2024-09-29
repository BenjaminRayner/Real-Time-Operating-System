#include "btree.h"

unsigned int get_buddy(unsigned int level, unsigned int position)
{
	return upow(2, level) - 1 + (position ^ 1);
}

unsigned int get_parent(unsigned int level, unsigned int position)
{
	return upow(2, level - 1) - 1 + (position / 2);
}

unsigned int get_left_child(unsigned int level, unsigned int position)
{
	return upow(2, level + 1) - 1 + (2 * position);
}

unsigned int get_right_child(unsigned int level, unsigned int position)
{
	return upow(2, level + 1) - 1 + (2 * position) + 1;
}

unsigned int get_position(mpool_t mpid, void *addr, unsigned int level)
{
	if ( mpid == MPID_IRAM1 ) {
		return ((unsigned int)addr - RAM1_START) / upow(2, MEM1_POWER - level);
	}
	
	if ( mpid == MPID_IRAM2 ) {
		return ((unsigned int)addr - RAM2_START) / upow(2, MEM2_POWER - level);
	}
	
	return 4294967295;
}

unsigned int get_index(unsigned int level, unsigned int position)
{
	unsigned int test = upow(2, level) - 1 + position;
	return upow(2, level) - 1 + position;
}

unsigned int get_offset(unsigned int index)
{
	return 7 - (((index / 8.0) - (index / 8)) * 8);
}

BOOL is_allocated(U8 *bit_tree, unsigned int index)
{
	return (bit_tree[index / 8] >> get_offset(index)) & 1U;
}

void clear_bit(U8 *bit_tree, unsigned int index)
{
	unsigned int offset = get_offset(index);
	bit_tree[index / 8] &= ~(1UL << offset);
}

void set_bit(U8 *bit_tree, unsigned int index)
{
	unsigned int offset = get_offset(index);
	bit_tree[index / 8] |= (1UL << offset);
}

void *get_address(mpool_t mpid, unsigned int level, unsigned int position)
{
	if ( mpid == MPID_IRAM1 ) {
		return (void *) (RAM1_START + (position * upow(2, MEM1_POWER) / upow(2, level)));
	}
	
	if ( mpid == MPID_IRAM2 ) {
		return (void *) (RAM2_START + (position * upow(2, MEM2_POWER) / upow(2, level)));
	}
	
	return NULL;
}

void *split_addr(mpool_t mpid, unsigned int level, void *start_addr)
{
	if ( mpid == MPID_IRAM1 ) {
		return (void *)((unsigned int)start_addr + (upow(2, MEM1_POWER - level) / 2));
	}
	
	if ( mpid == MPID_IRAM2 ) {
		return (void *)((unsigned int)start_addr + (upow(2, MEM2_POWER - level) / 2));
	}
	
	return NULL;
}
