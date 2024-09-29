/**
 * @brief The KCD Task Template File
 * @note  The file name and the function name can be changed
 * @see   k_tasks.h
 */

#include "rtx.h"
#include "k_task.h"
#include "uart_irq.h"
#include "k_mem.h"
#include "math.h"

// Mapping goes like this
// 0-9 mapped to 0-9
// A-Z mapped to 10-35
// a-z mapped to 36-61
U8 command_tid[62];
U8 cached_cmd[KCD_CMD_BUF_SIZE];
U8 cached_cmd_len = 0;
BOOL active_cmd = FALSE;

char LT_msg[] = "TID: x, STATE: x\n\r";
char LM_msg[] = "TID: x, STATE: x, FREE:      \n\r";
char cmd_nf[] = "Command not found.\n\r";
char cmd_inv[] = "Invalid command.\n\r";
U8 tid_index = 5;
U8 state_index = 15;
U8 free_index = 23;
U8 LT_msg_len = 18;
U8 LM_msg_len = 31;
U8 cmd_nf_len = 20;
U8 cmd_inv_len = 18;
 
void clear_cache() 
{
		for (int i = 0; i < KCD_CMD_BUF_SIZE; ++i) {
			cached_cmd[i] = 0;
		}
		
		active_cmd = FALSE;
		cached_cmd_len = 0;
}

U8* cmd_map(U8 key)
{				
		if (key >= '0' && key <= '9') {
			return &command_tid[key - '0'];
		}
		if (key >= 'A' && key <= 'Z') {
			return &command_tid[key - 'A' + 10];
		}
		if (key >= 'a' && key <= 'z') {
			return &command_tid[key - 'a' + 36];
		}
		
		return NULL;
}

void init_map() 
{
		for (int i = 0; i < 62; ++i) {
			command_tid[i] = 0;
		}
		
		U8* key_tid = cmd_map('L');
		*key_tid = TID_KCD;
}

U8* prep_disp_msg(U8 len)
{
		U8 *default_msg = k_mpool_alloc(MPID_IRAM2, MSG_HDR_SIZE + len);
					
		struct rtx_msg_hdr *ptr = (void *)default_msg;
		ptr->length = MSG_HDR_SIZE + len;
		ptr->sender_tid = TID_KCD;
		ptr->type = DISPLAY;
		default_msg += MSG_HDR_SIZE;
		
		return default_msg;
}

void run_LM()
{	
		U8 *default_msg = prep_disp_msg(LM_msg_len);							
		for (int i = 0; i < LM_msg_len; ++i) {
			default_msg[i] = LM_msg[i];
		}
						
		for (int i = 0; i < MAX_TASKS; ++i) {
							
			if (g_tcbs[i].state != DORMANT && g_tcbs[i].mb.buf_start != NULL) {
								
				default_msg[tid_index] = '0' + g_tcbs[i].tid;
				default_msg[state_index] = '0' + g_tcbs[i].state;	
				U8 digits = num_places(g_tcbs[i].mb.space);
				for (int j = 0; j < digits; ++j) {
					default_msg[free_index + digits - j] = '0' + get_digit(g_tcbs[i].mb.space, j);
				}
				for (int j = digits; j < 5; ++j) {
					default_msg[free_index + j + 1] = (char)0x20;
				}

				send_msg(TID_CON, default_msg - MSG_HDR_SIZE);
			}
		}
		k_mpool_dealloc(MPID_IRAM2, default_msg - MSG_HDR_SIZE);
}

void run_LT()
{
		U8 *default_msg = prep_disp_msg(LT_msg_len);
		for (int i = 0; i < LT_msg_len; ++i) {
			default_msg[i] = LT_msg[i];
		}
						
		for (int i = 0; i < MAX_TASKS; ++i) {
							
			if (g_tcbs[i].state != DORMANT) {
								
				default_msg[tid_index] = '0' + g_tcbs[i].tid;
				default_msg[state_index] = '0' + g_tcbs[i].state;
				send_msg(TID_CON, default_msg - MSG_HDR_SIZE);
			}
		}
		k_mpool_dealloc(MPID_IRAM2, default_msg - MSG_HDR_SIZE);
}

BOOL cmd_exist(U8* key_tid)
{
	if (key_tid) {
		if (g_tcbs[*key_tid].state != DORMANT && *key_tid != 0) {
			return TRUE;
		}
	}
	return FALSE;
}

void task_kcd(void)
{
		clear_cache();
		init_map();
    mbx_create(KCD_MBX_SIZE);
    U8 *msg = k_mpool_alloc(MPID_IRAM2, MSG_HDR_SIZE + 1);

    while (1) {
      recv_msg(msg, MSG_HDR_SIZE + 1);
      RTX_MSG_HDR *metadata = (void *)msg;
			U8 key = msg[MSG_HDR_SIZE];
			
			if (metadata->type == KCD_REG && key != 'L') {
				U8* key_tid = cmd_map(key);
				*key_tid = metadata->sender_tid;
			}
			
			if (metadata->type == KEY_IN) {
				metadata->type = DISPLAY;
				metadata->sender_tid = TID_KCD;
				send_msg(TID_CON, msg);
				
				if (key == '%') active_cmd = TRUE;
				else if (key == '\r') {
					msg[MSG_HDR_SIZE] = '\n';
					send_msg(TID_CON, msg);
					
					U8* key_tid = cmd_map(cached_cmd[0]);
					if (active_cmd && cmd_exist(key_tid)) { // Active cmd and exists
						
						if (cached_cmd[0] == 'L' && cached_cmd[1] == 'M' && cached_cmd_len == 2) run_LM();
						else if (cached_cmd[0] == 'L' && cached_cmd[1] == 'T' && cached_cmd_len == 2) run_LT();
						else if (cached_cmd[0] != 'L') { // Send cmd to task
							U8 *task_msg = k_mpool_alloc(MPID_IRAM2, MSG_HDR_SIZE + cached_cmd_len);
					
							struct rtx_msg_hdr *ptr = (void *)task_msg;
							ptr->length = MSG_HDR_SIZE + cached_cmd_len;
							ptr->sender_tid = TID_KCD;
							ptr->type = KCD_CMD;
							task_msg += MSG_HDR_SIZE;
							
							for (int i = 0; i < cached_cmd_len; ++i) {
								task_msg[i] = cached_cmd[i];
							}
							send_msg_nb(*key_tid, task_msg - MSG_HDR_SIZE);
							
							k_mpool_dealloc(MPID_IRAM2, task_msg - MSG_HDR_SIZE);
						}			
					}	
					else if (active_cmd) { // Active cmd and doesn't exist
						U8 *default_msg = prep_disp_msg(cmd_nf_len);						
						for (int i = 0; i < cmd_nf_len; ++i) {
							default_msg[i] = cmd_nf[i];
						}
						send_msg(TID_CON, default_msg - MSG_HDR_SIZE);
						
						k_mpool_dealloc(MPID_IRAM2, default_msg - MSG_HDR_SIZE);
					}
					else { // Not active cmd
						U8 *default_msg = prep_disp_msg(cmd_inv_len);
						for (int i = 0; i < cmd_inv_len; ++i) {
							default_msg[i] = cmd_inv[i];
						}
						send_msg(TID_CON, default_msg - MSG_HDR_SIZE);
						
						k_mpool_dealloc(MPID_IRAM2, default_msg - MSG_HDR_SIZE);
					}
					
					clear_cache();
				}
				else if (active_cmd == TRUE)  {
					cached_cmd[cached_cmd_len] = key;
					++cached_cmd_len;
					if (cached_cmd_len == KCD_CMD_BUF_SIZE) {
						active_cmd = FALSE;
					}
				}
			}
    }
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
