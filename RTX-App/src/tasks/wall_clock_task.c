/**
 * @brief The Wall Clock Display Task Template File 
 * @note  The file name and the function name can be changed
 * @see   k_tasks.h
 */

#include "rtx.h"
#include "math.h"
#include "printf.h"

U8 cmd_buf[MSG_HDR_SIZE + WCLCK_CMD_SIZE];
U8 clk_buf[40];

U8 hour = 0;
U8 minute = 0;
U8 second = 0;

BOOL clock_on = FALSE;
BOOL removed = TRUE;

void run_WR()
{
	hour = 0;
	minute = 0;
	second = 0;
	
	clock_on = TRUE;
	removed = FALSE;
}

void run_WS(char *new_time)
{	
	hour = (new_time[0] - '0') * 10;
	hour += new_time[1] - '0';
	minute = (new_time[3] - '0') * 10;
	minute += new_time[4] - '0';
	second = (new_time[6] - '0') * 10;
	second += new_time[7] - '0';
}

void run_WT()
{
	clk_buf[24] = ' ';
	clk_buf[25] = ' ';
	clk_buf[26] = ' ';
	clk_buf[27] = ' ';
	clk_buf[28] = ' ';
	clk_buf[29] = ' ';
	clk_buf[30] = ' ';
	clk_buf[31] = ' ';
		
	clock_on = FALSE;
}

void task_wall_clock(void)
{
		mbx_create(0x80);

		// Register command W
		RTX_MSG_HDR *ptr = (void *)cmd_buf;
		ptr->length = MSG_HDR_SIZE + 1;
		ptr->type = KCD_REG;
		ptr->sender_tid = TID_WCLCK;
		cmd_buf[MSG_HDR_SIZE] = 'W';
		send_msg(TID_KCD, (void *)ptr);
	
		// Init msg metadata
		ptr = (void *)clk_buf;
		ptr->length = 40;
		ptr->type = DISPLAY;
		sprintf((char *)&clk_buf[MSG_HDR_SIZE], "\033[s\033[H\33[2k");
		ptr->sender_tid = TID_WCLCK;
	
		// Turn into RT-task
    TIMEVAL tv;
		tv.sec = 1;
		tv.usec = 0;
		rt_tsk_set(&tv);

		while (1) {
			
			if (second++ >= 59) {
				second = 0;
				minute += 1; 
			} 
			if (minute >= 60) {
				minute = 0; 
				hour += 1; 
			} 
			if (hour > 23) { 
				hour = 0; 
			}
			
			recv_msg_nb(cmd_buf, MSG_HDR_SIZE + WCLCK_CMD_SIZE);
			RTX_MSG_HDR *metadata = (void *)cmd_buf;
			if (cmd_buf[MSG_HDR_SIZE] == 'W' && cmd_buf[MSG_HDR_SIZE + 1] == 'R' && metadata->length == 8) run_WR();
			else if (cmd_buf[MSG_HDR_SIZE] == 'W' && cmd_buf[MSG_HDR_SIZE + 1] == 'S' && metadata->length == 17) run_WS((void *)&cmd_buf[MSG_HDR_SIZE + 3]);
			else if (cmd_buf[MSG_HDR_SIZE] == 'W' && cmd_buf[MSG_HDR_SIZE + 1] == 'T' && metadata->length == 8) run_WT();
			cmd_buf[MSG_HDR_SIZE] = 0; // Clear cmd
			
			if (!removed) {
				
				if (clock_on) {
					sprintf((char *)&clk_buf[MSG_HDR_SIZE], "\033[s\033[H\033[72C\033[A\033[2K%c%c:%c%c:%c%c\033[u", '0' + get_digit(hour, 1), '0' + get_digit(hour, 0), '0' + get_digit(minute, 1), '0' + get_digit(minute, 0), '0' + get_digit(second, 1), '0' + get_digit(second, 0));
				}
				else {
					removed = TRUE;
				}	
				send_msg_nb(TID_CON, clk_buf);
			}

			rt_tsk_susp();
		}
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

