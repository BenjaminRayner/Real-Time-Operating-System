/**************************************************************************//**
 * @file        ae_tasks300.c
 * @brief       P3 Test Suite 300  - Basic Non-blocking Message Passing
 *
 * @version     V1.2021.07
 * @authors     Yiqing Huang
 * @date        2021 Jul
 *
 * @note        Each task is in an infinite loop. These Tasks never terminate.
 *
 *****************************************************************************/

#include "ae_tasks.h"
#include "uart_polling.h"
#include "printf.h"
#include "ae_util.h"
#include "ae_tasks_util.h"

/*
 *===========================================================================
 *                             MACROS
 *===========================================================================
 */

#define     NUM_TESTS       2       // number of tests
#define     NUM_INIT_TASKS  1       // number of tasks during initialization
#define     BUF_LEN         128     // receiver buffer length
#define     MY_MSG_TYPE     100     // some customized message type

/*
 *===========================================================================
 *                             GLOBAL VARIABLES
 *===========================================================================
 */
const char   PREFIX[]      = "G37-TS400";
const char   PREFIX_LOG[]  = "G37-TS400-LOG";
const char   PREFIX_LOG2[] = "G37-TS400-LOG2";
TASK_INIT    g_init_tasks[NUM_INIT_TASKS];

AE_XTEST     g_ae_xtest;                // test data, re-use for each test
AE_CASE      g_ae_cases[1];
AE_CASE_TSK  g_tsk_cases[NUM_TESTS];

/* The following arrays can also be dynamic allocated to reduce ZI-data size
 *  They do not have to be global buffers (provided the memory allocator has no bugs)
 */

U8 g_buf1[BUF_LEN];
U8 g_buf2[BUF_LEN];
task_t g_tasks[MAX_TASKS];
task_t g_tids[MAX_TASKS];

void set_ae_init_tasks (TASK_INIT **pp_tasks, int *p_num)
{
    *p_num = NUM_INIT_TASKS;
    *pp_tasks = g_init_tasks;
    set_ae_tasks(*pp_tasks, *p_num);
}

void set_ae_tasks(TASK_INIT *tasks, int num)
{
    for (int i = 0; i < num; i++ ) {
        tasks[i].u_stack_size = PROC_STACK_SIZE;
        tasks[i].prio = HIGH;
        tasks[i].priv = 0;
    }

    tasks[0].ptask = &task0;

    init_ae_tsk_test();
}

void init_ae_tsk_test(void)
{
    g_ae_xtest.test_id = 0;
    g_ae_xtest.index = 0;
    g_ae_xtest.num_tests = NUM_TESTS;
    g_ae_xtest.num_tests_run = 0;

    for ( int i = 1; i< NUM_TESTS; i++ ) {
        g_tsk_cases[i].p_ae_case = &g_ae_cases[i];
        g_tsk_cases[i].p_ae_case->results  = 0x0;
        g_tsk_cases[i].p_ae_case->test_id  = i;
        g_tsk_cases[i].p_ae_case->num_bits = 0;
        g_tsk_cases[i].pos = 0;  // first avaiable slot to write exec seq tid
        // *_expt fields are case specific, deligate to specific test case to initialize
    }
    printf("%s: START\r\n", PREFIX);
}

void update_ae_xtest(int test_id)
{
    g_ae_xtest.test_id = test_id;
    g_ae_xtest.index = 0;
    g_ae_xtest.num_tests_run++;
}

void gen_req0(int test_id)
{
    g_tsk_cases[test_id].p_ae_case->num_bits = 12;
    g_tsk_cases[test_id].p_ae_case->results = 0;
    g_tsk_cases[test_id].p_ae_case->test_id = test_id;
    g_tsk_cases[test_id].len = 16; // assign a value no greater than MAX_LEN_SEQ
    g_tsk_cases[test_id].pos_expt = 9;

    update_ae_xtest(test_id);
}

void gen_req1(int test_id)
{
    //bits[0:3] pos check, bits[4:12] for exec order check
    g_tsk_cases[test_id].p_ae_case->num_bits = 3; //Change to increase number of p_index
    g_tsk_cases[test_id].p_ae_case->results = 0;
    g_tsk_cases[test_id].p_ae_case->test_id = test_id;
    g_tsk_cases[test_id].len = 0;       // N/A for this test
    g_tsk_cases[test_id].pos_expt = 0;  // N/A for this test

    update_ae_xtest(test_id);
}

/**
 * @brief   task yield exec order test
 * @param   test_id, the test function ID
 * @param   ID of the test function that logs the testing data
 * @note    usually test data is logged by the same test function,
 *          but some time, we may have multiple tests to check the same test data
 *          logged by a particular test function
 */
void test1_start(int test_id, int test_id_data)
{
    gen_req1(1);

    U8      pos         = g_tsk_cases[test_id_data].pos;
    U8      pos_expt    = g_tsk_cases[test_id_data].pos_expt;
    task_t  *p_seq      = g_tsk_cases[test_id_data].seq;
    task_t  *p_seq_expt = g_tsk_cases[test_id_data].seq_expt;

    U8      *p_index    = &(g_ae_xtest.index);
    int     sub_result  = 0;

    (*p_index)++;
    sprintf(g_ae_xtest.msg, "seeing if not real time task raises EPERM", 128);
    int ret_val = rt_tsk_susp();
    sub_result = (errno == EPERM && ret_val == RTX_ERR) ? 1 : 0;
    process_sub_result(test_id, *p_index, sub_result);

    (*p_index)++;
    strcpy(g_ae_xtest.msg, "Seeing if elevating a non real time task doesn't raise EPERM");
    TIMEVAL tv;
    tv.sec = 0;
    tv.usec = 0;

    rt_tsk_set(&tv); //elevate to real-time
    ret_val = rt_tsk_set(&tv);  // try to set already-elevated task
    sub_result = (errno == EPERM && ret_val == RTX_ERR) ? 1 : 0;

    process_sub_result(test_id, *p_index, sub_result);

    (*p_index)++;
    strcpy(g_ae_xtest.msg, "Seeing if error is raised when we call rt_tsk_get on not real-time");
    TIMEVAL* buffer;
    ret_val = rt_tsk_get(8, buffer);
    sub_result = (errno == EINVAL && ret_val == RTX_ERR) ? 1 : 0;
    process_sub_result(test_id, *p_index, sub_result);

    test_exit();
}

int update_exec_seq(int test_id, task_t tid)
{

    U8 len = g_tsk_cases[test_id].len;
    U8 *p_pos = &g_tsk_cases[test_id].pos;
    task_t *p_seq = g_tsk_cases[test_id].seq;
    p_seq[*p_pos] = tid;
    (*p_pos)++;
    (*p_pos) = (*p_pos)%len;  // preventing out of array bound
    return RTX_OK;
}

/**************************************************************************//**
 * @brief   The first task to run in the system
 *****************************************************************************/

void task0(void)
{
    int ret_val = 10;
    //mbx_t mbx_id = -1;
    task_t tid = tsk_gettid();
    g_tids[0] = tid;
    int     test_id    = 0;
    U8      *p_index   = &(g_ae_xtest.index);
    int     sub_result = 0;

    printf("%s: TID = %u, task0 entering\r\n", PREFIX_LOG2, tid);
    update_exec_seq(test_id, tid);

    tsk_yield(); // let the other two tasks to run
    update_exec_seq(test_id, tid);
    test1_start(test_id + 1, test_id);
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
