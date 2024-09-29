
/**
 * @brief The Console Display Task Template File 
 * @note  The file name and the function name can be changed
 * @see   k_tasks.h
 */

#include "rtx.h"
#include "uart_irq.h"
#include "k_msg.h"
#include "k_mem.h"

void task_cdisp(void)
{	
	mbx_create(CON_MBX_SIZE);
	U8 *c_out = k_mpool_alloc(MPID_IRAM2, KCD_CMD_BUF_SIZE + MSG_HDR_SIZE);
	
	while (1) {
			recv_msg(c_out, KCD_CMD_BUF_SIZE + MSG_HDR_SIZE);
			RTX_MSG_HDR *metadata = (void *)c_out;
			if (metadata->length > uart_mb.space || !(metadata->type == DISPLAY)) {
				continue;
			}
			
			for (int i = MSG_HDR_SIZE; i < metadata->length; ++i) {
				enqueue(&uart_mb, c_out[i]);
			}
			
			LPC_UART_TypeDef *pUart = (LPC_UART_TypeDef *) LPC_UART0;
			while ( !mb_empty(&uart_mb) ) {
				pUart->IER |= IER_THRE;     			 												// turn on the TX interrupt to output mailbox contents		
			}
			pUart->IER &= ~IER_THRE;  			
	}
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

