#include "lpc1768_mem.h"
#include "common.h"
#include "math.h"

unsigned int get_buddy(unsigned int level, unsigned int position);
unsigned int get_parent(unsigned int level, unsigned int position);
unsigned int get_left_child(unsigned int level, unsigned int position);
unsigned int get_right_child(unsigned int level, unsigned int position);
unsigned int get_position(mpool_t mpid, void *addr, unsigned int level);
unsigned int get_index(unsigned int level, unsigned int position);
unsigned int get_offset(unsigned int index);
BOOL is_allocated(U8 *bit_tree, unsigned int index);
void clear_bit(U8 *bit_tree, unsigned int index);
void set_bit(U8 *bit_tree, unsigned int index);
void *get_address(mpool_t mpid, unsigned int level, unsigned int position);
void *split_addr(mpool_t mpid, unsigned int level, void *start_addr);
